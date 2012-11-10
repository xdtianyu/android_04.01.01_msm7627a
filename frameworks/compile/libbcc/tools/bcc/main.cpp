/*
 * Copyright 2010-2012, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(__HOST__)
  #if defined(__cplusplus)
    extern "C" {
  #endif
      extern char *TARGET_TRIPLE_STRING;
  #if defined(__cplusplus)
    };
  #endif
#else
#endif

#include <bcc/bcc.h>

#define DEFAULT_OUTPUT_FILENAME "a.out"

typedef int (*RootPtr)();

// This is a separate function so it can easily be set by breakpoint in gdb.
static int run(RootPtr rootFunc) {
  return rootFunc();
}

enum OutputType {
  OT_Executable,
  OT_Relocatable,
  OT_SharedObject
};

enum OutputType OutType = OT_Executable;
enum bccRelocModelEnum OutRelocModel = bccRelocDefault;
const char *InFile = NULL;
const char *OutFile = NULL;
const char *IntermediateOutFile = NULL;
bool RunRoot = false;

struct OptionInfo {
  const char *option_name;

  // min_option_argc is the minimum number of arguments this option should
  // have. This is for sanity check before invoking processing function.
  unsigned min_option_argc;

  const char *argument_desc;
  const char *help_message;

  // The function to process this option. Return the number of arguments it
  // consumed or < 0 if there's an error during the processing.
  int (*process)(int argc, char **arg);
};

// forward declaration of option processing functions
static int optSetTriple(int, char **);
static int optSetInput(int, char **);
static int optSetOutput(int, char **);
static int optSetIntermediateOutput(int, char **);
static int optOutputReloc(int, char **);
static int optSetOutputPIC(int, char **);
static int optSetOutputShared(int, char **);
static int optRunRoot(int, char **);
static int optHelp(int, char **);

static const struct OptionInfo Options[] = {
#if defined(__HOST__)
  { "C", 1, "triple", "set the triple string.",
    optSetTriple },
#endif

  { "c", 0, NULL, "compile and assemble, but do not link.",
    optOutputReloc },

  { "fPIC", 0, NULL,  "Generate position-independent code, if possible.",
    optSetOutputPIC },

  { "o", 1, "output", "write the native result to an output file.",
    optSetOutput },

  // FIXME: this option will be removed in the future when MCLinker is capable
  //        of generating shared library directly from given bitcode. It only
  //        takes effect when -shared is supplied.
  { "or", 1, NULL, "set the output filename for the intermediate relocatable.",
    optSetIntermediateOutput },


  { "shared", 0, NULL, "create a shared library.",
    optSetOutputShared },

  { "R", 0, NULL, "run root() method after a successful load and compile.",
    optRunRoot },

  { "h", 0, NULL, "print this help.",
    optHelp },
};
#define NUM_OPTIONS (sizeof(Options) / sizeof(struct OptionInfo))

static int parseOption(int argc, char** argv) {
  if (argc <= 1) {
    optHelp(argc, argv);
    return 0; // unreachable
  }

  // argv[i] is the current processing argument from command line
  int i = 1;
  while (i < argc) {
    const unsigned left_argc = argc - i - 1;

    if (argv[i][0] == '-') {
      // Find the corresponding OptionInfo object
      unsigned opt_idx = 0;
      while (opt_idx < NUM_OPTIONS) {
        if (::strcmp(&argv[i][1], Options[opt_idx].option_name) == 0) {
          const struct OptionInfo *cur_option = &Options[opt_idx];
          if (left_argc < cur_option->min_option_argc) {
            fprintf(stderr, "%s: '%s' requires at least %u arguments", argv[0],
                    cur_option->option_name, cur_option->min_option_argc);
            return 1;
          }

          int result = cur_option->process(left_argc, &argv[i]);
          if (result >= 0) {
            // consume the used arguments
            i += result;
          } else {
            // error occurs
            return 1;
          }

          break;
        }
        ++opt_idx;
      }
      if (opt_idx >= NUM_OPTIONS) {
        fprintf(stderr, "%s: unrecognized option '%s'", argv[0], argv[i]);
        return 1;
      }
    } else {
      if (InFile == NULL) {
        optSetInput(left_argc, &argv[i]);
      } else {
        fprintf(stderr, "%s: only a single input file is allowed currently.",
                argv[0]);
        return 1;
      }
    }
    i++;
  }

  return 0;
}

static BCCScriptRef loadScript() {
  if (!InFile) {
    fprintf(stderr, "input file required.\n");
    return NULL;
  }

  BCCScriptRef script = bccCreateScript();

  if (bccReadFile(script, InFile, /* flags */BCC_SKIP_DEP_SHA1) != 0) {
    fprintf(stderr, "bcc: FAILS to read bitcode.");
    bccDisposeScript(script);
    return NULL;
  }

  char *output = NULL;

  if (OutFile != NULL) {
    // Copy the outFile since we're going to modify it
    size_t outFileLen = strlen(OutFile);
    output = new char [outFileLen + 1];
    strncpy(output, OutFile, outFileLen);
  } else {
    if (OutType == OT_Executable) {
      output = new char [(sizeof(DEFAULT_OUTPUT_FILENAME) - 1) + 1];
      strncpy(output, DEFAULT_OUTPUT_FILENAME,
                  sizeof(DEFAULT_OUTPUT_FILENAME) - 1);
    } else {
      size_t inFileLen = strlen(InFile);
      output = new char [inFileLen + 3 /* ensure there's room for .so */ + 1];
      strncpy(output, InFile, inFileLen);

      char *fileExtension = strrchr(output, '.');
      if (fileExtension == NULL) {
        // append suffix
        fileExtension = output + inFileLen;
        *fileExtension = '.';
      }

      fileExtension++;  // skip '.'
      if (OutType == OT_Relocatable) {
        *fileExtension++ = 'o';
      } else /* must be OT_SharedObject */{
        *fileExtension++ = 's';
        *fileExtension++ = 'o';
      }
      *fileExtension++ = '\0';
    }
  }

  int bccResult = 0;
  const char *errMsg = NULL;
  switch (OutType) {
    case OT_Executable: {
      bccResult = 1;
      errMsg = "generation of executable is unsupported currently.";
      break;
    }
    case OT_Relocatable: {
      bccResult = bccPrepareRelocatable(script, output, OutRelocModel, 0);
      errMsg = "failed to generate relocatable.";
      break;
    }
    case OT_SharedObject: {
      if (IntermediateOutFile != NULL) {
        bccResult =
            bccPrepareRelocatable(script, IntermediateOutFile, bccRelocPIC, 0);
        errMsg = "failed to generate intermediate relocatable.";
      }

      if (bccResult == 0) {
        bccResult =
            bccPrepareSharedObject(script, IntermediateOutFile, output, 0);
        errMsg = "failed to generate shared library.";
      }
      break;
    }
  }

  delete [] output;

  if (bccResult == 0) {
    return script;
  } else {
    fprintf(stderr, "bcc: %s\n", errMsg);
    bccDisposeScript(script);
    return NULL;
  }
}

static int runRoot(BCCScriptRef script) {
  RootPtr rootPointer =
      reinterpret_cast<RootPtr>(bccGetFuncAddr(script, "main"));

  if (!rootPointer) {
    rootPointer = reinterpret_cast<RootPtr>(bccGetFuncAddr(script, "root"));
  }
  if (!rootPointer) {
    rootPointer = reinterpret_cast<RootPtr>(bccGetFuncAddr(script, "_Z4rootv"));
  }
  if (!rootPointer) {
    fprintf(stderr, "Could not find root or main or mangled root.\n");
    return 1;
  }

  fprintf(stderr, "Executing compiled code:\n");

  int result = run(rootPointer);
  fprintf(stderr, "result: %d\n", result);

  return 0;
}

int main(int argc, char** argv) {
  if(parseOption(argc, argv)) {
    return 1;
  }

  BCCScriptRef script;

  if((script = loadScript()) == NULL) {
    return 2;
  }

  if(RunRoot && runRoot(script)) {
    return 6;
  }

  bccDisposeScript(script);

  return 0;
}

/*
 * Functions to process the command line option.
 */
#if defined(__HOST__)
static int optSetTriple(int, char **arg) {
  TARGET_TRIPLE_STRING = arg[1];
  return 1;
}
#endif

static int optSetInput(int, char **arg) {
  // Check the input file path
  struct stat statInFile;
  if (stat(arg[0], &statInFile) < 0) {
    fprintf(stderr, "Unable to stat input file: %s\n", strerror(errno));
    return -1;
  }

  if (!S_ISREG(statInFile.st_mode)) {
    fprintf(stderr, "Input file should be a regular file.\n");
    return -1;
  }

  InFile = arg[0];
  return 0;
}

static int optSetOutput(int, char **arg) {
  char *lastSlash = strrchr(arg[1], '/');
  if ((lastSlash != NULL) && *(lastSlash + 1) == '\0') {
    fprintf(stderr, "bcc: output file should not be a directory.");
    return -1;
  }

  OutFile = arg[1];
  return 1;
}

static int optSetIntermediateOutput(int, char **arg) {
  char *lastSlash = strrchr(arg[1], '/');
  if ((lastSlash != NULL) && *(lastSlash + 1) == '\0') {
    fprintf(stderr, "bcc: output intermediate file should not be a directory.");
    return -1;
  }

  IntermediateOutFile = arg[1];
  return 1;
}

static int optOutputReloc(int, char **) {
  OutType = OT_Relocatable;
  return 0;
}

static int optSetOutputShared(int, char **) {
  OutType = OT_SharedObject;
  return 0;
}

static int optSetOutputPIC(int, char **) {
  OutRelocModel = bccRelocPIC;
  return 0;
}

static int optRunRoot(int, char **) {
  RunRoot = true;
  return 0;
}

static int optHelp(int, char **) {
  printf("Usage: bcc [OPTION]... [input file]\n\n");
  for (unsigned i = 0; i < NUM_OPTIONS; i++) {
    const struct OptionInfo *opt = &Options[i];

    printf("\t-%s", opt->option_name);
    if (opt->argument_desc)
      printf(" %s ", opt->argument_desc);
    else
      printf(" \t ");
    printf("\t%s\n", opt->help_message);
  }
  exit(0);
}

/*
 * Copyright 2010, The Android Open Source Project
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

#include "RuntimeStub.h"

#include <bcc/bcc_assert.h>

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
  const char *mName;
  void *mPtr;
} RuntimeFunction;

#if defined(__arm__) || defined(__mips__)
  #define DEF_GENERIC_RUNTIME(func)   \
    extern void *func;
  #define DEF_VFP_RUNTIME(func) \
    extern void *func ## vfp;
  #define DEF_LLVM_RUNTIME(func)
  #define DEF_BCC_RUNTIME(func)
#include "Runtime.def"
#endif

static const RuntimeFunction gRuntimes[] = {
#if defined(__arm__) || defined(__mips__)
  #define DEF_GENERIC_RUNTIME(func)   \
    { #func, (void*) &func },
  // TODO: enable only when target support VFP
  #define DEF_VFP_RUNTIME(func) \
    { #func, (void*) &func ## vfp },
#else
  // host compiler library must contain generic runtime
  #define DEF_GENERIC_RUNTIME(func)
  #define DEF_VFP_RUNTIME(func)
#endif
#define DEF_LLVM_RUNTIME(func)   \
  { #func, (void*) &func },
#define DEF_BCC_RUNTIME(func) \
  { #func, &func ## _bcc },
#include "Runtime.def"
};

static int CompareRuntimeFunction(const void *a, const void *b) {
  return strcmp(((const RuntimeFunction*) a)->mName,
               ((const RuntimeFunction*) b)->mName);
}

void *FindRuntimeFunction(const char *Name) {
  // binary search
  const RuntimeFunction Key = { Name, NULL };
  const RuntimeFunction *R =
      bsearch(&Key,
              gRuntimes,
              sizeof(gRuntimes) / sizeof(RuntimeFunction),
              sizeof(RuntimeFunction),
              CompareRuntimeFunction);

  return ((R) ? R->mPtr : NULL);
}

void VerifyRuntimesTable() {
  unsigned N = sizeof(gRuntimes) / sizeof(RuntimeFunction), i;
  for(i = 0; i < N; i++) {
    const char *Name = gRuntimes[i].mName;
    int *Ptr = FindRuntimeFunction(Name);

    if (Ptr != (int*) gRuntimes[i].mPtr)
      bccAssert(false && "Table is corrupted (runtime name should be sorted "
                         "in Runtime.def).");
  }
}

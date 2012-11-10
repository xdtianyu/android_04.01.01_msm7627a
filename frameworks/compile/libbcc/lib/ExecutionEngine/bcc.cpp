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

// Bitcode compiler (bcc) for Android:
//    This is an eager-compilation JIT running on Android.

#include <bcc/bcc.h>
#include "bcc_internal.h"

#include "Config.h"

#include "Compiler.h"
#include "DebugHelper.h"
#include "Script.h"

#include <string>

#include <utils/StopWatch.h>

#include <llvm/Support/CodeGen.h>

using namespace bcc;

namespace llvm {
  class Module;
}

static bool bccBuildStampPrinted = false;

static void bccPrintBuildStamp() {
  if (!bccBuildStampPrinted) {
    ALOGI("LIBBCC build time: %s", bccGetBuildTime());
    ALOGI("LIBBCC build revision: %s", bccGetBuildRev());
    bccBuildStampPrinted = true;
  }
}

extern "C" BCCScriptRef bccCreateScript() {
  BCC_FUNC_LOGGER();
  bccPrintBuildStamp();
  return wrap(new bcc::Script());
}


extern "C" void bccDisposeScript(BCCScriptRef script) {
  BCC_FUNC_LOGGER();
  delete unwrap(script);
}


extern "C" int bccRegisterSymbolCallback(BCCScriptRef script,
                                         BCCSymbolLookupFn pFn,
                                         void *pContext) {
  BCC_FUNC_LOGGER();
  return unwrap(script)->registerSymbolCallback(pFn, pContext);
}


extern "C" int bccGetError(BCCScriptRef script) {
  BCC_FUNC_LOGGER();
  return unwrap(script)->getError();
}

extern "C" int bccReadBC(BCCScriptRef script,
                         char const *resName,
                         char const *bitcode,
                         size_t bitcodeSize,
                         unsigned long flags) {
  BCC_FUNC_LOGGER();
  return unwrap(script)->addSourceBC(0, resName, bitcode, bitcodeSize, flags);
}


extern "C" int bccReadModule(BCCScriptRef script,
                             char const *resName /* deprecated */,
                             LLVMModuleRef module,
                             unsigned long flags) {
  BCC_FUNC_LOGGER();
  return unwrap(script)->addSourceModule(0, unwrap(module), flags);
}


extern "C" int bccReadFile(BCCScriptRef script,
                           char const *path,
                           unsigned long flags) {
  BCC_FUNC_LOGGER();
  return unwrap(script)->addSourceFile(0, path, flags);
}


extern "C" int bccLinkBC(BCCScriptRef script,
                         char const *resName,
                         char const *bitcode,
                         size_t bitcodeSize,
                         unsigned long flags) {
  BCC_FUNC_LOGGER();
  return unwrap(script)->addSourceBC(1, resName, bitcode, bitcodeSize, flags);
}


extern "C" int bccLinkFile(BCCScriptRef script,
                           char const *path,
                           unsigned long flags) {
  BCC_FUNC_LOGGER();
  return unwrap(script)->addSourceFile(1, path, flags);
}


extern "C" void bccMarkExternalSymbol(BCCScriptRef script, char const *name) {
  BCC_FUNC_LOGGER();
  unwrap(script)->markExternalSymbol(name);
}


extern "C" int bccPrepareRelocatable(BCCScriptRef script,
                                     char const *objPath,
                                     bccRelocModelEnum RelocModel,
                                     unsigned long flags) {
  BCC_FUNC_LOGGER();
  llvm::Reloc::Model RM;

  switch (RelocModel) {
    case bccRelocDefault: {
      RM = llvm::Reloc::Default;
      break;
    }
    case bccRelocStatic: {
      RM = llvm::Reloc::Static;
      break;
    }
    case bccRelocPIC: {
      RM = llvm::Reloc::PIC_;
      break;
    }
    case bccRelocDynamicNoPIC: {
      RM = llvm::Reloc::DynamicNoPIC;
      break;
    }
    default: {
      ALOGE("Unrecognized relocation model for bccPrepareObject!");
      return BCC_INVALID_VALUE;
    }
  }

  return unwrap(script)->prepareRelocatable(objPath, RM, flags);
}


extern "C" int bccPrepareSharedObject(BCCScriptRef script,
                                      char const *objPath,
                                      char const *dsoPath,
                                      unsigned long flags) {
  BCC_FUNC_LOGGER();
  return unwrap(script)->prepareSharedObject(objPath, dsoPath, flags);
}


extern "C" int bccPrepareExecutable(BCCScriptRef script,
                                    char const *cacheDir,
                                    char const *cacheName,
                                    unsigned long flags) {
  BCC_FUNC_LOGGER();

  android::StopWatch compileTimer("bcc: PrepareExecutable time");

  return unwrap(script)->prepareExecutable(cacheDir, cacheName, flags);
}


extern "C" void *bccGetFuncAddr(BCCScriptRef script, char const *funcname) {
  BCC_FUNC_LOGGER();

  void *addr = unwrap(script)->lookup(funcname);

#if DEBUG_BCC_REFLECT
  ALOGD("Function Address: %s --> %p\n", funcname, addr);
#endif

  return addr;
}


extern "C" void bccGetExportVarList(BCCScriptRef script,
                                    size_t varListSize,
                                    void **varList) {
  BCC_FUNC_LOGGER();

  if (varList) {
    unwrap(script)->getExportVarList(varListSize, varList);

#if DEBUG_BCC_REFLECT
    size_t count = unwrap(script)->getExportVarCount();
    ALOGD("ExportVarCount = %lu\n", (unsigned long)count);

    if (count > varListSize) {
      count = varListSize;
    }

    for (size_t i = 0; i < count; ++i) {
      ALOGD("ExportVarList[%lu] = %p\n", (unsigned long)i, varList[i]);
    }
#endif
  }
}


extern "C" void bccGetExportFuncList(BCCScriptRef script,
                                     size_t funcListSize,
                                     void **funcList) {
  BCC_FUNC_LOGGER();

  if (funcList) {
    unwrap(script)->getExportFuncList(funcListSize, funcList);

#if DEBUG_BCC_REFLECT
    size_t count = unwrap(script)->getExportFuncCount();
    ALOGD("ExportFuncCount = %lu\n", (unsigned long)count);

    if (count > funcListSize) {
      count = funcListSize;
    }

    for (size_t i = 0; i < count; ++i) {
      ALOGD("ExportFuncList[%lu] = %p\n", (unsigned long)i, funcList[i]);
    }
#endif
  }
}


extern "C" void bccGetExportForEachList(BCCScriptRef script,
                                        size_t forEachListSize,
                                        void **forEachList) {
  BCC_FUNC_LOGGER();

  if (forEachList) {
    unwrap(script)->getExportForEachList(forEachListSize, forEachList);

#if DEBUG_BCC_REFLECT
    size_t count = unwrap(script)->getExportForEachCount();
    ALOGD("ExportForEachCount = %lu\n", (unsigned long)count);

    if (count > forEachListSize) {
      count = forEachListSize;
    }

    for (size_t i = 0; i < count; ++i) {
      ALOGD("ExportForEachList[%lu] = %p\n", (unsigned long)i, forEachList[i]);
    }
#endif
  }
}

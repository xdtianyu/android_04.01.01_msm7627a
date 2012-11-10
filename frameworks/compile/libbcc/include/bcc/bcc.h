/*
 * Copyright (C) 2010-2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_BCC_BCC_H
#define ANDROID_BCC_BCC_H

#include <stddef.h>
#include <stdint.h>

/*-------------------------------------------------------------------------*/

/* libbcc script opaque type */
typedef struct BCCOpaqueScript *BCCScriptRef;


/* Symbol lookup function type */
typedef void *(*BCCSymbolLookupFn)(void *context, char const *symbolName);


/* llvm::Module (see <llvm>/include/llvm-c/Core.h for details) */
typedef struct LLVMOpaqueModule *LLVMModuleRef;


/*-------------------------------------------------------------------------*/


#define BCC_NO_ERROR          0x0000
#define BCC_INVALID_ENUM      0x0500
#define BCC_INVALID_OPERATION 0x0502
#define BCC_INVALID_VALUE     0x0501
#define BCC_OUT_OF_MEMORY     0x0505


/*-------------------------------------------------------------------------*/


/* Optional Flags for bccReadBC, bccReadFile, bccLinkBC, bccLinkFile */
#define BCC_SKIP_DEP_SHA1 (1 << 0)


/*-------------------------------------------------------------------------*/

/*
 * Relocation model when prepare object, it provides 1-1 mapping to the enum
 * llvm::Reloc::Model in llvm/Support/CodeGen.h
 */
typedef enum bccRelocModelEnum {
  bccRelocDefault,  // Use default target-defined relocation model
  bccRelocStatic,
  bccRelocPIC,
  bccRelocDynamicNoPIC
} bccRelocModelEnum;

/*-------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

BCCScriptRef bccCreateScript();

void bccDisposeScript(BCCScriptRef script);

int bccRegisterSymbolCallback(BCCScriptRef script,
                              BCCSymbolLookupFn pFn,
                              void *pContext);

int bccGetError(BCCScriptRef script); /* deprecated */



int bccReadBC(BCCScriptRef script,
              char const *resName,
              char const *bitcode,
              size_t bitcodeSize,
              unsigned long flags);

int bccReadModule(BCCScriptRef script,
                  char const *resName,
                  LLVMModuleRef module,
                  unsigned long flags);

int bccReadFile(BCCScriptRef script,
                char const *path,
                unsigned long flags);

int bccLinkBC(BCCScriptRef script,
              char const *resName,
              char const *bitcode,
              size_t bitcodeSize,
              unsigned long flags);

int bccLinkFile(BCCScriptRef script,
                char const *path,
                unsigned long flags);

void bccMarkExternalSymbol(BCCScriptRef script, char const *name);

int bccPrepareRelocatable(BCCScriptRef script,
                          char const *objPath,
                          bccRelocModelEnum RelocModel,
                          unsigned long flags);

int bccPrepareSharedObject(BCCScriptRef script,
                           char const *objPath,
                           char const *dsoPath,
                           unsigned long flags);

int bccPrepareExecutable(BCCScriptRef script,
                         char const *cacheDir,
                         char const *cacheName,
                         unsigned long flags);

void *bccGetFuncAddr(BCCScriptRef script, char const *funcname);

void bccGetExportVarList(BCCScriptRef script,
                         size_t varListSize,
                         void **varList);

void bccGetExportFuncList(BCCScriptRef script,
                          size_t funcListSize,
                          void **funcList);

void bccGetExportForEachList(BCCScriptRef script,
                             size_t forEachListSize,
                             void **forEachList);

char const *bccGetBuildTime();

char const *bccGetBuildRev();

char const *bccGetBuildSHA1();

#ifdef __cplusplus
};
#endif

/*-------------------------------------------------------------------------*/

#endif

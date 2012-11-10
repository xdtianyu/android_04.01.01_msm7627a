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

#include "Config.h"

#include "ScriptCached.h"

#include "DebugHelper.h"

#include <stdlib.h>

namespace bcc {

ScriptCached::~ScriptCached() {
  // Deallocate string pool, exported var list, exported func list
  if (mpStringPoolRaw) { free(mpStringPoolRaw); }
  if (mpExportVars) { free(mpExportVars); }
  if (mpExportFuncs) { free(mpExportFuncs); }
  if (mpExportForEach) { free(mpExportForEach); }
  if (mpObjectSlotList) { free(mpObjectSlotList); }
}

void ScriptCached::getExportVarList(size_t varListSize, void **varList) {
  if (varList) {
    size_t varCount = getExportVarCount();

    if (varCount > varListSize) {
      varCount = varListSize;
    }

    memcpy(varList, mpExportVars->cached_addr_list, sizeof(void *) * varCount);
  }
}


void ScriptCached::getExportFuncList(size_t funcListSize, void **funcList) {
  if (funcList) {
    size_t funcCount = getExportFuncCount();

    if (funcCount > funcListSize) {
      funcCount = funcListSize;
    }

    memcpy(funcList, mpExportFuncs->cached_addr_list,
           sizeof(void *) * funcCount);
  }
}


void ScriptCached::getExportForEachList(size_t forEachListSize,
                                        void **forEachList) {
  if (forEachList) {
    size_t forEachCount = getExportForEachCount();

    if (forEachCount > forEachListSize) {
      forEachCount = forEachListSize;
    }

    memcpy(forEachList, mpExportForEach->cached_addr_list,
           sizeof(void *) * forEachCount);
  }
}


void ScriptCached::getPragmaList(size_t pragmaListSize,
                                 char const **keyList,
                                 char const **valueList) {
  size_t pragmaCount = getPragmaCount();

  if (pragmaCount > pragmaListSize) {
    pragmaCount = pragmaListSize;
  }

  if (keyList) {
    for (size_t i = 0; i < pragmaCount; ++i) {
      *keyList++ = mPragmas[i].first;
    }
  }

  if (valueList) {
    for (size_t i = 0; i < pragmaCount; ++i) {
      *valueList++ = mPragmas[i].second;
    }
  }
}


void ScriptCached::getObjectSlotList(size_t objectSlotListSize,
                                     uint32_t *objectSlotList) {
  if (objectSlotList) {
    size_t objectSlotCount = getObjectSlotCount();

    if (objectSlotCount > objectSlotListSize) {
      objectSlotCount = objectSlotListSize;
    }

    memcpy(objectSlotList, mpObjectSlotList->object_slot_list,
           sizeof(uint32_t) * objectSlotCount);
  }
}


void *ScriptCached::lookup(const char *name) {
  return rsloaderGetSymbolAddress(mRSExecutable, name);
}

void ScriptCached::getFuncInfoList(size_t funcInfoListSize,
                                   FuncInfo *funcInfoList) {
  if (funcInfoList) {
    size_t funcCount = getFuncCount();

    if (funcCount > funcInfoListSize) {
      funcCount = funcInfoListSize;
    }

    FuncInfo *info = funcInfoList;
    for (FuncTable::const_iterator
         I = mFunctions.begin(), E = mFunctions.end();
         I != E && funcCount > 0; ++I, ++info, --funcCount) {
      info->name = I->first.c_str();
      info->addr = I->second.first;
      info->size = I->second.second;
    }
  }
}


} // namespace bcc

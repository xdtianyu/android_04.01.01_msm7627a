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

#include "ScriptCompiled.h"

#include "bcc_internal.h"
#include "DebugHelper.h"

namespace bcc {

ScriptCompiled::~ScriptCompiled() {
}

void ScriptCompiled::getExportVarList(size_t varListSize, void **varList) {
  if (varList) {
    size_t varCount = getExportVarCount();

    if (varCount > varListSize) {
      varCount = varListSize;
    }

    for (ExportVarList::const_iterator
         I = mExportVars.begin(), E = mExportVars.end();
         I != E && varCount > 0; ++I, --varCount) {
      *varList++ = *I;
    }
  }
}

void ScriptCompiled::getExportVarNameList(std::vector<std::string> &varList) {
  varList = mExportVarsName;
}


void ScriptCompiled::getExportFuncNameList(std::vector<std::string> &funcList) {
  funcList = mExportFuncsName;
}


void ScriptCompiled::getExportForEachNameList(std::vector<std::string> &forEachList) {
  forEachList = mExportForEachName;
}


void ScriptCompiled::getExportFuncList(size_t funcListSize, void **funcList) {
  if (funcList) {
    size_t funcCount = getExportFuncCount();

    if (funcCount > funcListSize) {
      funcCount = funcListSize;
    }

    for (ExportFuncList::const_iterator
         I = mExportFuncs.begin(), E = mExportFuncs.end();
         I != E && funcCount > 0; ++I, --funcCount) {
      *funcList++ = *I;
    }
  }
}


void ScriptCompiled::getExportForEachList(size_t forEachListSize,
                                          void **forEachList) {
  if (forEachList) {
    size_t forEachCount = getExportForEachCount();

    if (forEachCount > forEachListSize) {
      forEachCount = forEachListSize;
    }

    for (ExportForEachList::const_iterator
         I = mExportForEach.begin(), E = mExportForEach.end();
         I != E && forEachCount > 0; ++I, --forEachCount) {
      *forEachList++ = *I;
    }
  }
}


void ScriptCompiled::getPragmaList(size_t pragmaListSize,
                                   char const **keyList,
                                   char const **valueList) {
  size_t pragmaCount = getPragmaCount();

  if (pragmaCount > pragmaListSize) {
    pragmaCount = pragmaListSize;
  }

  for (PragmaList::const_iterator
       I = mPragmas.begin(), E = mPragmas.end();
       I != E && pragmaCount > 0; ++I, --pragmaCount) {
    if (keyList) { *keyList++ = I->first.c_str(); }
    if (valueList) { *valueList++ = I->second.c_str(); }
  }
}


void *ScriptCompiled::lookup(const char *name) {
  return mCompiler.getSymbolAddress(name);
}


void ScriptCompiled::getFuncInfoList(size_t funcInfoListSize,
                                     FuncInfo *funcInfoList) {
  if (funcInfoList) {
    size_t funcCount = getFuncCount();

    if (funcCount > funcInfoListSize) {
      funcCount = funcInfoListSize;
    }

    FuncInfo *info = funcInfoList;
    for (FuncInfoMap::const_iterator
         I = mEmittedFunctions.begin(), E = mEmittedFunctions.end();
         I != E && funcCount > 0; ++I, ++info, --funcCount) {
      info->name = I->first.c_str();
      info->addr = I->second->addr;
      info->size = I->second->size;
    }
  }
}

void ScriptCompiled::getObjectSlotList(size_t objectSlotListSize,
                                       uint32_t *objectSlotList) {
  if (objectSlotList) {
    size_t objectSlotCount = getObjectSlotCount();

    if (objectSlotCount > objectSlotListSize) {
      objectSlotCount = objectSlotListSize;
    }

    for (ObjectSlotList::const_iterator
         I = mObjectSlots.begin(), E = mObjectSlots.end();
         I != E && objectSlotCount > 0; ++I, --objectSlotCount) {
      *objectSlotList++ = *I;
    }
  }

}

} // namespace bcc

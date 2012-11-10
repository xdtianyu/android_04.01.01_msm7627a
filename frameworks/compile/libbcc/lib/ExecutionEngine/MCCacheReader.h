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

#ifndef BCC_MCCACHEREADER_H
#define BCC_MCCACHEREADER_H

#include "ScriptCached.h"

#include <llvm/ADT/OwningPtr.h>

#include <map>
#include <string>
#include <utility>

#include <stddef.h>
#include <stdint.h>

struct MCO_Header;

namespace bcc {
  class FileHandle;
  class Script;

  class MCCacheReader {
  private:
    FileHandle *mObjFile, *mInfoFile;
    off_t mInfoFileSize;

    MCO_Header *mpHeader;
    MCO_DependencyTable *mpCachedDependTable;
    MCO_PragmaList *mpPragmaList;
    MCO_FuncTable *mpFuncTable;

    MCO_String_Ptr *mpVarNameList;
    MCO_String_Ptr *mpFuncNameList;
    MCO_String_Ptr *mpForEachNameList;

    llvm::OwningPtr<ScriptCached> mpResult;

    std::map<std::string,
             std::pair<uint32_t, unsigned char const *> > mDependencies;

    bool mIsContextSlotNotAvail;

    BCCSymbolLookupFn mpSymbolLookupFn;
    void *mpSymbolLookupContext;

  public:
    MCCacheReader()
      : mObjFile(NULL), mInfoFile(NULL), mInfoFileSize(0), mpHeader(NULL),
        mpCachedDependTable(NULL), mpPragmaList(NULL),
        mpVarNameList(NULL), mpFuncNameList(NULL), mpForEachNameList(NULL),
        mIsContextSlotNotAvail(false) {
    }

    ~MCCacheReader();

    void addDependency(MCO_ResourceType resType,
                       std::string const &resName,
                       unsigned char const *sha1) {
      mDependencies.insert(std::make_pair(resName,
                           std::make_pair((uint32_t)resType, sha1)));
    }

    ScriptCached *readCacheFile(FileHandle *objFile, FileHandle *infoFile, Script *s);
    bool checkCacheFile(FileHandle *objFile, FileHandle *infoFile, Script *S);

    bool isContextSlotNotAvail() const {
      return mIsContextSlotNotAvail;
    }

    void registerSymbolCallback(BCCSymbolLookupFn pFn, void *pContext) {
      mpSymbolLookupFn = pFn;
      mpSymbolLookupContext = pContext;
    }

  private:
    bool readHeader();
    bool readStringPool();
    bool readDependencyTable();
    bool readPragmaList();
    bool readObjectSlotList();
    bool readObjFile();
    bool readRelocationTable();

    bool readVarNameList();
    bool readFuncNameList();
    bool readForEachNameList();

    bool checkFileSize();
    bool checkHeader();
    bool checkMachineIntType();
    bool checkSectionOffsetAndSize();
    bool checkStringPool();
    bool checkDependency();
    bool checkContext();

    bool relocate();

    static void *resolveSymbolAdapter(void *context, char const *name);

  };

} // namespace bcc

#endif // BCC_MCCACHEREADER_H

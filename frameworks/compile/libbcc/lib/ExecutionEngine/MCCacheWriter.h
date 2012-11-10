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

#ifndef BCC_MCCACHEWRITER_H
#define BCC_MCCACHEWRITER_H

#include <bcc/bcc_mccache.h>

#include "FileHandle.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bcc {
  class Script;

  class MCCacheWriter {
  private:
    Script *mpOwner;

    FileHandle *mObjFile, *mInfoFile;

    std::vector<std::pair<char const *, size_t> > mStringPool;

    std::map<std::string,
             std::pair<uint32_t, unsigned char const *> > mDependencies;

    MCO_Header *mpHeaderSection;
    MCO_StringPool *mpStringPoolSection;
    MCO_DependencyTable *mpDependencyTableSection;
    MCO_PragmaList *mpPragmaListSection;
    MCO_ObjectSlotList *mpObjectSlotSection;

    MCO_String_Ptr *mpExportVarNameListSection;
    MCO_String_Ptr *mpExportFuncNameListSection;
    MCO_String_Ptr *mpExportForEachNameListSection;

    std::vector<std::string> varNameList;
    std::vector<std::string> funcNameList;
    std::vector<std::string> forEachNameList;

  public:
    MCCacheWriter()
      : mpHeaderSection(NULL), mpStringPoolSection(NULL),
        mpDependencyTableSection(NULL), mpPragmaListSection(NULL),
        mpObjectSlotSection(NULL) {
    }

    ~MCCacheWriter();

    bool writeCacheFile(FileHandle *objFile, FileHandle *infoFile,
                        Script *S, uint32_t libRS_threadable);

    void addDependency(MCO_ResourceType resType,
                       std::string const &resName,
                       unsigned char const *sha1) {
      mDependencies.insert(std::make_pair(resName,
                           std::make_pair((uint32_t)resType, sha1)));
    }

  private:
    bool prepareHeader(uint32_t libRS_threadable);
    bool prepareStringPool();
    bool prepareDependencyTable();
    bool prepareRelocationTable();
    bool preparePragmaList();
    bool prepareObjectSlotList();

    bool prepareExportVarNameList();
    bool prepareExportFuncNameList();
    bool prepareExportForEachNameList();

    bool writeAll();

    bool calcSectionOffset();

    size_t addString(char const *str, size_t size) {
      mStringPool.push_back(std::make_pair(str, size));
      return mStringPool.size() - 1;
    }

  };

} // namespace bcc

#endif // BCC_MCCACHEWRITER_H

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

#include "MCCacheWriter.h"

#include "DebugHelper.h"
#include "FileHandle.h"
#include "Script.h"

#include <map>
#include <string>
#include <vector>
#include <utility>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

namespace bcc {

MCCacheWriter::~MCCacheWriter() {
#define CHECK_AND_FREE(VAR) if (VAR) { free(VAR); }

  CHECK_AND_FREE(mpHeaderSection);
  CHECK_AND_FREE(mpStringPoolSection);
  CHECK_AND_FREE(mpDependencyTableSection);
  CHECK_AND_FREE(mpPragmaListSection);
  CHECK_AND_FREE(mpObjectSlotSection);
  CHECK_AND_FREE(mpExportVarNameListSection);
  CHECK_AND_FREE(mpExportFuncNameListSection);

#undef CHECK_AND_FREE
}

bool MCCacheWriter::writeCacheFile(FileHandle *objFile, FileHandle *infoFile,
                                 Script *S, uint32_t libRS_threadable) {
  if (!objFile || objFile->getFD() < 0 || !infoFile || infoFile->getFD() < 0) {
    return false;
  }

  mObjFile = objFile;
  mInfoFile = infoFile;
  mpOwner = S;

  bool result = prepareHeader(libRS_threadable)
             && prepareDependencyTable()
             && preparePragmaList()
             && prepareExportVarNameList()
             && prepareExportFuncNameList()
             && prepareExportForEachNameList()
             && prepareStringPool()
             && prepareObjectSlotList()
             && calcSectionOffset()
             && writeAll()
             ;

  return result;
}


bool MCCacheWriter::prepareHeader(uint32_t libRS_threadable) {
  MCO_Header *header = (MCO_Header *)malloc(sizeof(MCO_Header));

  if (!header) {
    ALOGE("Unable to allocate for header.\n");
    return false;
  }

  mpHeaderSection = header;

  // Initialize
  memset(header, '\0', sizeof(MCO_Header));

  // Magic word and version
  memcpy(header->magic, MCO_MAGIC, 4);
  memcpy(header->version, MCO_VERSION, 4);

  // Machine Integer Type
  uint32_t number = 0x00000001;
  header->endianness = (*reinterpret_cast<char *>(&number) == 1) ? 'e' : 'E';
  header->sizeof_off_t = sizeof(off_t);
  header->sizeof_size_t = sizeof(size_t);
  header->sizeof_ptr_t = sizeof(void *);

  // libRS is threadable dirty hack
  // TODO: This should be removed in the future
  header->libRS_threadable = libRS_threadable;

  return true;
}


bool MCCacheWriter::prepareDependencyTable() {
  size_t tableSize = sizeof(MCO_DependencyTable) +
                     sizeof(MCO_Dependency) * mDependencies.size();

  MCO_DependencyTable *tab = (MCO_DependencyTable *)malloc(tableSize);

  if (!tab) {
    ALOGE("Unable to allocate for dependency table section.\n");
    return false;
  }

  mpDependencyTableSection = tab;
  mpHeaderSection->depend_tab_size = tableSize;

  tab->count = mDependencies.size();

  size_t i = 0;
  for (map<string, pair<uint32_t, unsigned char const *> >::iterator
       I = mDependencies.begin(), E = mDependencies.end(); I != E; ++I, ++i) {
    MCO_Dependency *dep = &tab->table[i];

    dep->res_name_strp_index = addString(I->first.c_str(), I->first.size());
    dep->res_type = I->second.first;
    memcpy(dep->sha1, I->second.second, 20);
  }

  return true;
}

bool MCCacheWriter::preparePragmaList() {
  size_t pragmaCount = mpOwner->getPragmaCount();

  size_t listSize = sizeof(MCO_PragmaList) +
                    sizeof(MCO_Pragma) * pragmaCount;

  MCO_PragmaList *list = (MCO_PragmaList *)malloc(listSize);

  if (!list) {
    ALOGE("Unable to allocate for pragma list\n");
    return false;
  }

  mpPragmaListSection = list;
  mpHeaderSection->pragma_list_size = listSize;

  list->count = pragmaCount;

  vector<char const *> keyList(pragmaCount);
  vector<char const *> valueList(pragmaCount);
  mpOwner->getPragmaList(pragmaCount, &*keyList.begin(), &*valueList.begin());

  for (size_t i = 0; i < pragmaCount; ++i) {
    char const *key = keyList[i];
    char const *value = valueList[i];

    size_t keyLen = strlen(key);
    size_t valueLen = strlen(value);

    MCO_Pragma *pragma = &list->list[i];
    pragma->key_strp_index = addString(key, keyLen);
    pragma->value_strp_index = addString(value, valueLen);
  }

  return true;
}

bool MCCacheWriter::prepareStringPool() {
  // Calculate string pool size
  size_t size = sizeof(MCO_StringPool) +
                sizeof(MCO_String) * mStringPool.size();

  off_t strOffset = size;

  for (size_t i = 0; i < mStringPool.size(); ++i) {
    size += mStringPool[i].second + 1;
  }

  // Create string pool
  MCO_StringPool *pool = (MCO_StringPool *)malloc(size);

  if (!pool) {
    ALOGE("Unable to allocate string pool.\n");
    return false;
  }

  mpStringPoolSection = pool;
  mpHeaderSection->str_pool_size = size;

  pool->count = mStringPool.size();

  char *strPtr = reinterpret_cast<char *>(pool) + strOffset;

  for (size_t i = 0; i < mStringPool.size(); ++i) {
    MCO_String *str = &pool->list[i];

    str->length = mStringPool[i].second;
    str->offset = strOffset;
    memcpy(strPtr, mStringPool[i].first, str->length);

    strPtr += str->length;
    *strPtr++ = '\0';

    strOffset += str->length + 1;
  }

  return true;
}


bool MCCacheWriter::prepareExportVarNameList() {
  size_t varCount = mpOwner->getExportVarCount();
  size_t listSize = sizeof(MCO_String_Ptr) + sizeof(size_t) * varCount;

  MCO_String_Ptr *list = (MCO_String_Ptr*)malloc(listSize);

  if (!list) {
    ALOGE("Unable to allocate for export variable name list\n");
    return false;
  }

  mpExportVarNameListSection = list;
  mpHeaderSection->export_var_name_list_size = listSize;

  list->count = static_cast<size_t>(varCount);

  mpOwner->getExportVarNameList(varNameList);
  for (size_t i = 0; i < varCount; ++i) {
    list->strp_indexs[i] = addString(varNameList[i].c_str(), varNameList[i].length());
  }
  return true;
}


bool MCCacheWriter::prepareExportFuncNameList() {
  size_t funcCount = mpOwner->getExportFuncCount();
  size_t listSize = sizeof(MCO_String_Ptr) + sizeof(size_t) * funcCount;

  MCO_String_Ptr *list = (MCO_String_Ptr*)malloc(listSize);

  if (!list) {
    ALOGE("Unable to allocate for export function name list\n");
    return false;
  }

  mpExportFuncNameListSection = list;
  mpHeaderSection->export_func_name_list_size = listSize;

  list->count = static_cast<size_t>(funcCount);

  mpOwner->getExportFuncNameList(funcNameList);
  for (size_t i = 0; i < funcCount; ++i) {
    list->strp_indexs[i] = addString(funcNameList[i].c_str(), funcNameList[i].length());
  }
  return true;
}


bool MCCacheWriter::prepareExportForEachNameList() {
  size_t forEachCount = mpOwner->getExportForEachCount();
  size_t listSize = sizeof(MCO_String_Ptr) + sizeof(size_t) * forEachCount;

  MCO_String_Ptr *list = (MCO_String_Ptr*)malloc(listSize);

  if (!list) {
    ALOGE("Unable to allocate for export forEach name list\n");
    return false;
  }

  mpExportForEachNameListSection = list;
  mpHeaderSection->export_foreach_name_list_size = listSize;

  list->count = static_cast<size_t>(forEachCount);

  mpOwner->getExportForEachNameList(forEachNameList);
  for (size_t i = 0; i < forEachCount; ++i) {
    list->strp_indexs[i] = addString(forEachNameList[i].c_str(), forEachNameList[i].length());
  }
  return true;
}


bool MCCacheWriter::prepareObjectSlotList() {
  size_t objectSlotCount = mpOwner->getObjectSlotCount();

  size_t listSize = sizeof(MCO_ObjectSlotList) +
                    sizeof(uint32_t) * objectSlotCount;

  MCO_ObjectSlotList *list = (MCO_ObjectSlotList *)malloc(listSize);

  if (!list) {
    ALOGE("Unable to allocate for object slot list\n");
    return false;
  }

  mpObjectSlotSection = list;
  mpHeaderSection->object_slot_list_size = listSize;

  list->count = objectSlotCount;

  mpOwner->getObjectSlotList(objectSlotCount, list->object_slot_list);
  return true;
}


bool MCCacheWriter::calcSectionOffset() {
  size_t offset = sizeof(MCO_Header);

#define OFFSET_INCREASE(NAME)                                               \
  do {                                                                      \
    /* Align to a word */                                                   \
    size_t rem = offset % sizeof(int);                                      \
    if (rem > 0) {                                                          \
      offset += sizeof(int) - rem;                                          \
    }                                                                       \
                                                                            \
    /* Save the offset and increase it */                                   \
    mpHeaderSection->NAME##_offset = offset;                                \
    offset += mpHeaderSection->NAME##_size;                                 \
  } while (0)

  OFFSET_INCREASE(str_pool);
  OFFSET_INCREASE(depend_tab);
  OFFSET_INCREASE(pragma_list);
  OFFSET_INCREASE(func_table);
  OFFSET_INCREASE(object_slot_list);
  OFFSET_INCREASE(export_var_name_list);
  OFFSET_INCREASE(export_func_name_list);
  OFFSET_INCREASE(export_foreach_name_list);

#undef OFFSET_INCREASE

  return true;
}


bool MCCacheWriter::writeAll() {
#define WRITE_SECTION(NAME, OFFSET, SIZE, SECTION)                          \
  do {                                                                      \
    if (mInfoFile->seek(OFFSET, SEEK_SET) == -1) {                          \
      ALOGE("Unable to seek to " #NAME " section for writing.\n");           \
      return false;                                                         \
    }                                                                       \
                                                                            \
    if (mInfoFile->write(reinterpret_cast<char *>(SECTION), (SIZE)) !=      \
        static_cast<ssize_t>(SIZE)) {                                       \
      ALOGE("Unable to write " #NAME " section to cache file.\n");           \
      return false;                                                         \
    }                                                                       \
  } while (0)

#define WRITE_SECTION_SIMPLE(NAME, SECTION)                                 \
  WRITE_SECTION(NAME,                                                       \
                mpHeaderSection->NAME##_offset,                             \
                mpHeaderSection->NAME##_size,                               \
                SECTION)

  WRITE_SECTION(header, 0, sizeof(MCO_Header), mpHeaderSection);

  WRITE_SECTION_SIMPLE(str_pool, mpStringPoolSection);
  WRITE_SECTION_SIMPLE(depend_tab, mpDependencyTableSection);
  WRITE_SECTION_SIMPLE(pragma_list, mpPragmaListSection);
  WRITE_SECTION_SIMPLE(object_slot_list, mpObjectSlotSection);

  WRITE_SECTION_SIMPLE(export_var_name_list, mpExportVarNameListSection);
  WRITE_SECTION_SIMPLE(export_func_name_list, mpExportFuncNameListSection);
  WRITE_SECTION_SIMPLE(export_foreach_name_list, mpExportForEachNameListSection);

#undef WRITE_SECTION_SIMPLE
#undef WRITE_SECTION

  if (static_cast<size_t>(mObjFile->write(mpOwner->getELF(),
                                          mpOwner->getELFSize()))
      != mpOwner->getELFSize()) {
      ALOGE("Unable to write ELF to cache file.\n");
      return false;
  }

  return true;
}

} // namespace bcc

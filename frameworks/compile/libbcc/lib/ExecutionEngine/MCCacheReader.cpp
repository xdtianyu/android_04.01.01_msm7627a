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

#include "MCCacheReader.h"

#include "DebugHelper.h"
#include "FileHandle.h"
#include "ScriptCached.h"
#include "Runtime.h"

#include <bcc/bcc_mccache.h>

#include <llvm/ADT/OwningPtr.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <utility>
#include <vector>

#include <new>

#include <stdlib.h>
#include <string.h>

using namespace std;

namespace bcc {

MCCacheReader::~MCCacheReader() {
  if (mpHeader) { free(mpHeader); }
  if (mpCachedDependTable) { free(mpCachedDependTable); }
  if (mpPragmaList) { free(mpPragmaList); }
  if (mpVarNameList) { free(mpVarNameList); }
  if (mpFuncNameList) { free(mpFuncNameList); }
}

ScriptCached *MCCacheReader::readCacheFile(FileHandle *objFile,
                                           FileHandle *infoFile,
                                           Script *S) {
  bool result = checkCacheFile(objFile, infoFile, S)
             && readPragmaList()
             && readObjectSlotList()
             && readObjFile()
             && readVarNameList()
             && readFuncNameList()
             && readForEachNameList()
             //&& relocate()
             ;

  return result ? mpResult.take() : NULL;
}

bool MCCacheReader::checkCacheFile(FileHandle *objFile,
                                            FileHandle *infoFile,
                                            Script *S) {
  // Check file handle
  if (!objFile || objFile->getFD() < 0 || !infoFile || infoFile->getFD() < 0) {
    return false;
  }

  mObjFile = objFile;
  mInfoFile = infoFile;

  // Allocate ScriptCached object
  mpResult.reset(new (nothrow) ScriptCached(S));

  if (!mpResult) {
    ALOGE("Unable to allocate ScriptCached object.\n");
    return false;
  }

  bool result = checkFileSize()
             && readHeader()
             && checkHeader()
             && checkMachineIntType()
             && checkSectionOffsetAndSize()
             && readStringPool()
             && checkStringPool()
             && readDependencyTable()
             && checkDependency()
             ;

  return result;
}


bool MCCacheReader::checkFileSize() {
  struct stat stfile;
  if (fstat(mInfoFile->getFD(), &stfile) < 0) {
    ALOGE("Unable to stat cache file.\n");
    return false;
  }

  mInfoFileSize = stfile.st_size;

  if (mInfoFileSize < (off_t)sizeof(MCO_Header)) {
    ALOGE("Cache file is too small to be correct.\n");
    return false;
  }

  return true;
}


bool MCCacheReader::readHeader() {
  if (mInfoFile->seek(0, SEEK_SET) != 0) {
    ALOGE("Unable to seek to 0. (reason: %s)\n", strerror(errno));
    return false;
  }

  mpHeader = (MCO_Header *)malloc(sizeof(MCO_Header));
  if (!mpHeader) {
    ALOGE("Unable to allocate for cache header.\n");
    return false;
  }

  if (mInfoFile->read(reinterpret_cast<char *>(mpHeader), sizeof(MCO_Header)) !=
      (ssize_t)sizeof(MCO_Header)) {
    ALOGE("Unable to read cache header.\n");
    return false;
  }

  // Dirty hack for libRS.
  // TODO(all): This should be removed in the future.
  if (mpHeader->libRS_threadable) {
    mpResult->mLibRSThreadable = true;
  }

  return true;
}


bool MCCacheReader::checkHeader() {
  if (memcmp(mpHeader->magic, MCO_MAGIC, 4) != 0) {
    ALOGE("Bad magic word\n");
    return false;
  }

  if (memcmp(mpHeader->version, MCO_VERSION, 4) != 0) {
    mpHeader->version[4 - 1] = '\0'; // ensure c-style string terminated
    ALOGI("Cache file format version mismatch: now %s cached %s\n",
         MCO_VERSION, mpHeader->version);
    return false;
  }
  return true;
}


bool MCCacheReader::checkMachineIntType() {
  uint32_t number = 0x00000001;

  bool isLittleEndian = (*reinterpret_cast<char *>(&number) == 1);
  if ((isLittleEndian && mpHeader->endianness != 'e') ||
      (!isLittleEndian && mpHeader->endianness != 'E')) {
    ALOGE("Machine endianness mismatch.\n");
    return false;
  }

  if ((unsigned int)mpHeader->sizeof_off_t != sizeof(off_t) ||
      (unsigned int)mpHeader->sizeof_size_t != sizeof(size_t) ||
      (unsigned int)mpHeader->sizeof_ptr_t != sizeof(void *)) {
    ALOGE("Machine integer size mismatch.\n");
    return false;
  }

  return true;
}


bool MCCacheReader::checkSectionOffsetAndSize() {
#define CHECK_SECTION_OFFSET(NAME)                                          \
  do {                                                                      \
    off_t offset = mpHeader-> NAME##_offset;                                \
    off_t size = (off_t)mpHeader-> NAME##_size;                             \
                                                                            \
    if (mInfoFileSize < offset || mInfoFileSize < offset + size) {          \
      ALOGE(#NAME " section overflow.\n");                                   \
      return false;                                                         \
    }                                                                       \
                                                                            \
    if (offset % sizeof(int) != 0) {                                        \
      ALOGE(#NAME " offset must aligned to %d.\n", (int)sizeof(int));        \
      return false;                                                         \
    }                                                                       \
                                                                            \
    if (size < static_cast<off_t>(sizeof(size_t))) {                        \
      ALOGE(#NAME " size is too small to be correct.\n");                    \
      return false;                                                         \
    }                                                                       \
  } while (0)

  CHECK_SECTION_OFFSET(str_pool);
  CHECK_SECTION_OFFSET(depend_tab);
  //CHECK_SECTION_OFFSET(reloc_tab);
  CHECK_SECTION_OFFSET(pragma_list);

#undef CHECK_SECTION_OFFSET

  return true;
}


#define CACHE_READER_READ_SECTION(TYPE, AUTO_MANAGED_HOLDER, NAME)          \
  TYPE *NAME##_raw = (TYPE *)malloc(mpHeader->NAME##_size);                 \
                                                                            \
  if (!NAME##_raw) {                                                        \
    ALOGE("Unable to allocate for " #NAME "\n");                             \
    return false;                                                           \
  }                                                                         \
                                                                            \
  /* We have to ensure that some one will deallocate NAME##_raw */          \
  AUTO_MANAGED_HOLDER = NAME##_raw;                                         \
                                                                            \
  if (mInfoFile->seek(mpHeader->NAME##_offset, SEEK_SET) == -1) {           \
    ALOGE("Unable to seek to " #NAME " section\n");                          \
    return false;                                                           \
  }                                                                         \
                                                                            \
  if (mInfoFile->read(reinterpret_cast<char *>(NAME##_raw),                 \
                  mpHeader->NAME##_size) != (ssize_t)mpHeader->NAME##_size) \
  {                                                                         \
    ALOGE("Unable to read " #NAME ".\n");                                    \
    return false;                                                           \
  }


bool MCCacheReader::readStringPool() {
  CACHE_READER_READ_SECTION(MCO_StringPool,
                            mpResult->mpStringPoolRaw, str_pool);

  char *str_base = reinterpret_cast<char *>(str_pool_raw);

  vector<char const *> &pool = mpResult->mStringPool;
  for (size_t i = 0; i < str_pool_raw->count; ++i) {
    char *str = str_base + str_pool_raw->list[i].offset;
    pool.push_back(str);
  }

  return true;
}


bool MCCacheReader::checkStringPool() {
  MCO_StringPool *poolR = mpResult->mpStringPoolRaw;
  vector<char const *> &pool = mpResult->mStringPool;

  // Ensure that every c-style string is ended with '\0'
  for (size_t i = 0; i < poolR->count; ++i) {
    if (pool[i][poolR->list[i].length] != '\0') {
      ALOGE("The %lu-th string does not end with '\\0'.\n", (unsigned long)i);
      return false;
    }
  }

  return true;
}


bool MCCacheReader::readDependencyTable() {
  CACHE_READER_READ_SECTION(MCO_DependencyTable, mpCachedDependTable,
                            depend_tab);
  return true;
}


bool MCCacheReader::checkDependency() {
  if (mDependencies.size() != mpCachedDependTable->count) {
    ALOGE("Dependencies count mismatch. (%lu vs %lu)\n",
         (unsigned long)mDependencies.size(),
         (unsigned long)mpCachedDependTable->count);
    return false;
  }

  vector<char const *> &strPool = mpResult->mStringPool;
  map<string, pair<uint32_t, unsigned char const *> >::iterator dep;

  dep = mDependencies.begin();
  for (size_t i = 0; i < mpCachedDependTable->count; ++i, ++dep) {
    string const &depName = dep->first;
    uint32_t depType = dep->second.first;
    unsigned char const *depSHA1 = dep->second.second;

    MCO_Dependency *depCached =&mpCachedDependTable->table[i];
    char const *depCachedName = strPool[depCached->res_name_strp_index];
    uint32_t depCachedType = depCached->res_type;
    unsigned char const *depCachedSHA1 = depCached->sha1;

    if (depName != depCachedName) {
      ALOGE("Cache dependency name mismatch:\n");
      ALOGE("  given:  %s\n", depName.c_str());
      ALOGE("  cached: %s\n", depCachedName);

      return false;
    }

    if (memcmp(depSHA1, depCachedSHA1, 20) != 0) {
      ALOGE("Cache dependency %s sha1 mismatch:\n", depCachedName);

#define PRINT_SHA1(PREFIX, X, POSTFIX) \
      ALOGE(PREFIX "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x" \
                  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x" POSTFIX, \
           X[0], X[1], X[2], X[3], X[4], X[5], X[6], X[7], X[8], X[9], \
           X[10],X[11],X[12],X[13],X[14],X[15],X[16],X[17],X[18],X[19]);

      PRINT_SHA1("  given:  ", depSHA1, "\n");
      PRINT_SHA1("  cached: ", depCachedSHA1, "\n");

#undef PRINT_SHA1

      return false;
    }

    if (depType != depCachedType) {
      ALOGE("Cache dependency %s resource type mismatch.\n", depCachedName);
      return false;
    }
  }

  return true;
}

bool MCCacheReader::readVarNameList() {
  CACHE_READER_READ_SECTION(MCO_String_Ptr, mpVarNameList, export_var_name_list);
  vector<char const *> const &strPool = mpResult->mStringPool;

  mpResult->mpExportVars = (MCO_ExportVarList*)
                            malloc(sizeof(size_t) +
                                   sizeof(void*) * export_var_name_list_raw->count);
  if (!mpResult->mpExportVars) {
    ALOGE("Unable to allocate for mpExportVars\n");
    return false;
  }
  mpResult->mpExportVars->count = export_var_name_list_raw->count;

  for (size_t i = 0; i < export_var_name_list_raw->count; ++i) {
    mpResult->mpExportVars->cached_addr_list[i] =
      rsloaderGetSymbolAddress(mpResult->mRSExecutable, strPool[export_var_name_list_raw->strp_indexs[i]]);
#if DEBUG_MC_REFLECT
    ALOGD("Get symbol address: %s -> %p",
      strPool[export_var_name_list_raw->strp_indexs[i]], mpResult->mpExportVars->cached_addr_list[i]);
#endif
  }
  return true;
}

bool MCCacheReader::readFuncNameList() {
  CACHE_READER_READ_SECTION(MCO_String_Ptr, mpFuncNameList, export_func_name_list);
  vector<char const *> const &strPool = mpResult->mStringPool;

  mpResult->mpExportFuncs = (MCO_ExportFuncList*)
                            malloc(sizeof(size_t) +
                                   sizeof(void*) * export_func_name_list_raw->count);
  if (!mpResult->mpExportFuncs) {
    ALOGE("Unable to allocate for mpExportFuncs\n");
    return false;
  }
  mpResult->mpExportFuncs->count = export_func_name_list_raw->count;

  for (size_t i = 0; i < export_func_name_list_raw->count; ++i) {
    mpResult->mpExportFuncs->cached_addr_list[i] =
      rsloaderGetSymbolAddress(mpResult->mRSExecutable, strPool[export_func_name_list_raw->strp_indexs[i]]);
#if DEBUG_MC_REFLECT
    ALOGD("Get function address: %s -> %p",
      strPool[export_func_name_list_raw->strp_indexs[i]], mpResult->mpExportFuncs->cached_addr_list[i]);
#endif
  }
  return true;
}

bool MCCacheReader::readForEachNameList() {
  CACHE_READER_READ_SECTION(MCO_String_Ptr, mpForEachNameList, export_foreach_name_list);
  vector<char const *> const &strPool = mpResult->mStringPool;

  mpResult->mpExportForEach = (MCO_ExportForEachList*)
                              malloc(sizeof(size_t) +
                                     sizeof(void*) * export_foreach_name_list_raw->count);
  if (!mpResult->mpExportForEach) {
    ALOGE("Unable to allocate for mpExportForEach\n");
    return false;
  }
  mpResult->mpExportForEach->count = export_foreach_name_list_raw->count;

  for (size_t i = 0; i < export_foreach_name_list_raw->count; ++i) {
    mpResult->mpExportForEach->cached_addr_list[i] =
      rsloaderGetSymbolAddress(mpResult->mRSExecutable, strPool[export_foreach_name_list_raw->strp_indexs[i]]);
#if DEBUG_MC_REFLECT
    ALOGE("Get foreach function address: %s -> %p",
      strPool[export_foreach_name_list_raw->strp_indexs[i]], mpResult->mpExportForEach->cached_addr_list[i]);
#endif
  }
  return true;
}

bool MCCacheReader::readPragmaList() {
  CACHE_READER_READ_SECTION(MCO_PragmaList, mpPragmaList, pragma_list);

  vector<char const *> const &strPool = mpResult->mStringPool;
  ScriptCached::PragmaList &pragmas = mpResult->mPragmas;

  for (size_t i = 0; i < pragma_list_raw->count; ++i) {
    MCO_Pragma *pragma = &pragma_list_raw->list[i];
    pragmas.push_back(make_pair(strPool[pragma->key_strp_index],
                                strPool[pragma->value_strp_index]));
  }

  return true;
}


bool MCCacheReader::readObjectSlotList() {
  CACHE_READER_READ_SECTION(MCO_ObjectSlotList,
                            mpResult->mpObjectSlotList, object_slot_list);
  return true;
}

void *MCCacheReader::resolveSymbolAdapter(void *context, char const *name) {
  MCCacheReader *self = reinterpret_cast<MCCacheReader *>(context);

  if (void *Addr = FindRuntimeFunction(name)) {
    return Addr;
  }

  if (self->mpSymbolLookupFn) {
    if (void *Addr =
        self->mpSymbolLookupFn(self->mpSymbolLookupContext, name)) {
      return Addr;
    }
  }

  ALOGE("Unable to resolve symbol: %s\n", name);
  return NULL;
}

bool MCCacheReader::readObjFile() {
  if (mpResult->mCachedELFExecutable.size() != 0) {
    ALOGE("Attempted to read cached object into a non-empty script");
    return false;
  }
  char readBuffer[1024];
  int readSize;
  while ((readSize = mObjFile->read(readBuffer, 1024)) > 0) {
    mpResult->mCachedELFExecutable.append(readBuffer, readBuffer + readSize);
  }
  if (readSize != 0) {
    ALOGE("Read file Error");
    return false;
  }
  ALOGD("Read object file size %d", (int)mpResult->mCachedELFExecutable.size());
  mpResult->mRSExecutable =
  rsloaderCreateExec((unsigned char *)&*(mpResult->mCachedELFExecutable.begin()),
                     mpResult->mCachedELFExecutable.size(),
                     &resolveSymbolAdapter, this);

  // Point ELF section headers to location of executable code, otherwise
  // execution through GDB stops unexpectedly as GDB translates breakpoints
  // in JITted code incorrectly (and complains about being unable to insert
  // breakpoint at an invalid address)
  rsloaderUpdateSectionHeaders(mpResult->mRSExecutable,
    (unsigned char*) mpResult->mCachedELFExecutable.begin());

  return true;
}

#undef CACHE_READER_READ_SECTION

bool MCCacheReader::readRelocationTable() {
  // TODO(logan): Not finished.
  return true;
}


bool MCCacheReader::relocate() {
  return true;
}

} // namespace bcc

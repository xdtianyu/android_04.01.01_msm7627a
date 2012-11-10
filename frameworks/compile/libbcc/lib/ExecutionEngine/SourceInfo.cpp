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

#include "SourceInfo.h"

#include "MCCacheWriter.h"
#include "MCCacheReader.h"

#include "DebugHelper.h"
#include "ScriptCompiled.h"
#include "Sha1Helper.h"

#include <bcc/bcc.h>

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/system_error.h>

#include <stddef.h>
#include <string.h>

namespace bcc {


SourceInfo *SourceInfo::createFromBuffer(char const *resName,
                                         char const *bitcode,
                                         size_t bitcodeSize,
                                         unsigned long flags) {
  SourceInfo *result = new SourceInfo();

  if (!result) {
    return NULL;
  }

  result->type = SourceKind::Buffer;
  result->buffer.resName = resName;
  result->buffer.bitcode = bitcode;
  result->buffer.bitcodeSize = bitcodeSize;
  result->flags = flags;

  if (!resName && !(flags & BCC_SKIP_DEP_SHA1)) {
    result->flags |= BCC_SKIP_DEP_SHA1;

    ALOGW("It is required to give resName for sha1 dependency check.\n");
    ALOGW("Sha1sum dependency check will be skipped.\n");
    ALOGW("Set BCC_SKIP_DEP_SHA1 for flags to surpress this warning.\n");
  }

  if (result->flags & BCC_SKIP_DEP_SHA1) {
    memset(result->sha1, '\0', 20);
  } else {
    calcSHA1(result->sha1, bitcode, bitcodeSize);
  }

  return result;
}


SourceInfo *SourceInfo::createFromFile(char const *path,
                                       unsigned long flags) {
  SourceInfo *result = new SourceInfo();

  if (!result) {
    return NULL;
  }

  result->type = SourceKind::File;
  result->file.path = path;
  result->flags = flags;

  memset(result->sha1, '\0', 20);

  if (!(result->flags & BCC_SKIP_DEP_SHA1)) {
    calcFileSHA1(result->sha1, path);
  }

  return result;
}


SourceInfo *SourceInfo::createFromModule(llvm::Module *module,
                                         unsigned long flags) {
  SourceInfo *result = new SourceInfo();

  if (!result) {
    return NULL;
  }

  result->type = SourceKind::Module;
  result->module = module;
  result->flags = flags;

  if (! (flags & BCC_SKIP_DEP_SHA1)) {
    result->flags |= BCC_SKIP_DEP_SHA1;

    ALOGW("Unable to calculate sha1sum for llvm::Module.\n");
    ALOGW("Sha1sum dependency check will be skipped.\n");
    ALOGW("Set BCC_SKIP_DEP_SHA1 for flags to surpress this warning.\n");
  }

  memset(result->sha1, '\0', 20);

  return result;
}


int SourceInfo::prepareModule(llvm::LLVMContext *context) {
  if (module)
    return 0;

  llvm::OwningPtr<llvm::MemoryBuffer> mem;
  std::string errmsg;

  switch (type) {
  case SourceKind::Buffer:
    {
      mem.reset(llvm::MemoryBuffer::getMemBuffer(
          llvm::StringRef(buffer.bitcode, buffer.bitcodeSize), "", false));

      if (!mem.get()) {
        ALOGE("Unable to MemoryBuffer::getMemBuffer(addr=%p, size=%lu)\n",
              buffer.bitcode, (unsigned long)buffer.bitcodeSize);
        return 1;
      }
    }
    break;

  case SourceKind::File:
    {
      if (llvm::error_code ec = llvm::MemoryBuffer::getFile(file.path, mem)) {
        ALOGE("Unable to MemoryBuffer::getFile(path=%s, %s)\n",
              file.path, ec.message().c_str());
        return 1;
      }
    }
    break;

  default:
    return 0;
    break;
  }

  if (context)
    shared_context = true;
  else
    context = new llvm::LLVMContext();

  module = llvm::ParseBitcodeFile(mem.get(), *context, &errmsg);
  if (module == NULL) {
    ALOGE("Unable to ParseBitcodeFile: %s\n", errmsg.c_str());
    if (!shared_context)
      delete context;
  }

  return (module == NULL);
}

SourceInfo::~SourceInfo() {
  if (module != NULL) {
    llvm::LLVMContext *context = &module->getContext();
    delete module;
    if (!shared_context)
      delete context;
  }
}

template <typename T> void SourceInfo::introDependency(T &checker) {
  if (flags & BCC_SKIP_DEP_SHA1) {
    return;
  }

  switch (type) {
  case SourceKind::Buffer:
    checker.addDependency(BCC_APK_RESOURCE, buffer.resName, sha1);
    break;

  case SourceKind::File:
    checker.addDependency(BCC_FILE_RESOURCE, file.path, sha1);
    break;

  default:
    break;
  }
}

template void SourceInfo::introDependency<MCCacheWriter>(MCCacheWriter &);
template void SourceInfo::introDependency<MCCacheReader>(MCCacheReader &);


} // namespace bcc

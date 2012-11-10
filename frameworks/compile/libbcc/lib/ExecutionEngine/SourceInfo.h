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

#ifndef BCC_SOURCEINFO_H
#define BCC_SOURCEINFO_H

#include "Config.h"

#include <llvm/Module.h>

#include <stddef.h>

namespace llvm {
  class LLVMContext;
}

namespace bcc {
  namespace SourceKind {
    enum SourceType {
      File,
      Buffer,
      Module,
    };
  }

  class SourceInfo {
  private:
    SourceKind::SourceType type;

    // Note: module should not be a part of union.  Since, we are going to
    // use module to store the pointer to parsed bitcode.
    llvm::Module *module;
    // If true, the LLVM context behind the module is shared with others.
    // Therefore, don't try to destroy the context it when destroy the module.
    bool shared_context;

    union {
      struct {
        char const *resName;
        char const *bitcode;
        size_t bitcodeSize;
      } buffer;

      struct {
        char const *path;
      } file;
    };

    unsigned long flags;

    unsigned char sha1[20];

  private:
    SourceInfo() : module(NULL), shared_context(false) { }

  public:
    static SourceInfo *createFromBuffer(char const *resName,
                                        char const *bitcode,
                                        size_t bitcodeSize,
                                        unsigned long flags);

    static SourceInfo *createFromFile(char const *path,
                                      unsigned long flags);

    static SourceInfo *createFromModule(llvm::Module *module,
                                        unsigned long flags);

    inline llvm::Module *getModule() const {
      return module;
    }

    inline llvm::LLVMContext *getContext() const {
      return (module) ? &module->getContext() : NULL;
    }

    // Share with the given context if it's provided.
    int prepareModule(llvm::LLVMContext *context = NULL);

    template <typename T> void introDependency(T &checker);

    ~SourceInfo();
  };


} // namespace bcc

#endif // BCC_SOURCEINFO_H

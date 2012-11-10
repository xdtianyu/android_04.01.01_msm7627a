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

#ifndef BCC_COMPILER_H
#define BCC_COMPILER_H

#include <bcc/bcc.h>

#include <Config.h>

#include "librsloader.h"

#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Target/TargetMachine.h"

#include <stddef.h>

#include <list>
#include <string>
#include <vector>
#include <utility>


namespace llvm {
  class Module;
  class NamedMDNode;
  class TargetData;
}


namespace bcc {
  class ScriptCompiled;
  struct CompilerOption;

  class Compiler {
  private:
    //////////////////////////////////////////////////////////////////////////
    // The variable section below (e.g., Triple, CodeGenOptLevel)
    // is initialized in GlobalInitialization()
    //
    static bool GlobalInitialized;

    // If given, this will be the name of the target triple to compile for.
    // If not given, the initial values defined in this file will be used.
    static std::string Triple;
    static llvm::Triple::ArchType ArchType;

    static llvm::CodeGenOpt::Level CodeGenOptLevel;

    // End of section of GlobalInitializing variables
    /////////////////////////////////////////////////////////////////////////
    // If given, the name of the target CPU to generate code for.
    static std::string CPU;

    // The list of target specific features to enable or disable -- this should
    // be a list of strings starting with '+' (enable) or '-' (disable).
    static std::vector<std::string> Features;

    static void LLVMErrorHandler(void *UserData, const std::string &Message);

    friend class CodeEmitter;
    friend class CodeMemoryManager;

  private:
    ScriptCompiled *mpResult;

    std::string mError;

    // Compilation buffer for MC
    llvm::SmallVector<char, 1024> mEmittedELFExecutable;

    // Loaded and relocated executable
    RSExecRef mRSExecutable;

    BCCSymbolLookupFn mpSymbolLookupFn;
    void *mpSymbolLookupContext;

    llvm::Module *mModule;

    bool mHasLinked;

  public:
    Compiler(ScriptCompiled *result);

    static void GlobalInitialization();

    static std::string const &getTargetTriple() {
      return Triple;
    }

    static llvm::Triple::ArchType getTargetArchType() {
      return ArchType;
    }

    void registerSymbolCallback(BCCSymbolLookupFn pFn, void *pContext) {
      mpSymbolLookupFn = pFn;
      mpSymbolLookupContext = pContext;
    }

    void *getSymbolAddress(char const *name);

    const llvm::SmallVector<char, 1024> &getELF() const {
      return mEmittedELFExecutable;
    }

    int readModule(llvm::Module *module) {
      mModule = module;
      return hasError();
    }

    int linkModule(llvm::Module *module);

    int compile(const CompilerOption &option);

    char const *getErrorMessage() {
      return mError.c_str();
    }

    const llvm::Module *getModule() const {
      return mModule;
    }

    ~Compiler();

  private:

    int runCodeGen(llvm::TargetData *TD, llvm::TargetMachine *TM,
                   llvm::NamedMDNode const *ExportVarMetadata,
                   llvm::NamedMDNode const *ExportFuncMetadata);

    int runMCCodeGen(llvm::TargetData *TD, llvm::TargetMachine *TM);

    static void *resolveSymbolAdapter(void *context, char const *name);

    int runInternalPasses(std::vector<std::string>& Names,
                          std::vector<uint32_t>& Signatures);

    int runLTO(llvm::TargetData *TD,
               std::vector<const char*>& ExportSymbols,
               llvm::CodeGenOpt::Level OptimizationLevel);

    bool hasError() const {
      return !mError.empty();
    }

    void setError(const char *Error) {
      mError.assign(Error);  // Copying
    }

    void setError(const std::string &Error) {
      mError = Error;
    }

  };  // End of class Compiler

} // namespace bcc

#endif // BCC_COMPILER_H

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

#include "Compiler.h"

#include "Config.h"
#include <bcinfo/MetadataExtractor.h>

#if USE_DISASSEMBLER
#include "Disassembler/Disassembler.h"
#endif

#include "DebugHelper.h"
#include "FileHandle.h"
#include "Runtime.h"
#include "ScriptCompiled.h"
#include "Sha1Helper.h"
#include "CompilerOption.h"

#include "librsloader.h"

#include "Transforms/BCCTransforms.h"

#include "llvm/ADT/StringRef.h"

#include "llvm/Analysis/Passes.h"

#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/SchedulerRegistry.h"

#include "llvm/MC/MCContext.h"
#include "llvm/MC/SubtargetFeature.h"

#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"

#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Constants.h"
#include "llvm/GlobalValue.h"
#include "llvm/Linker.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Type.h"
#include "llvm/Value.h"

#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

extern char* gDebugDumpDirectory;
namespace bcc {

//////////////////////////////////////////////////////////////////////////////
// BCC Compiler Static Variables
//////////////////////////////////////////////////////////////////////////////

bool Compiler::GlobalInitialized = false;


#if !defined(__HOST__)
  #define TARGET_TRIPLE_STRING  DEFAULT_TARGET_TRIPLE_STRING
#else
// In host TARGET_TRIPLE_STRING is a variable to allow cross-compilation.
  #if defined(__cplusplus)
    extern "C" {
  #endif
      char *TARGET_TRIPLE_STRING = (char*)DEFAULT_TARGET_TRIPLE_STRING;
  #if defined(__cplusplus)
    };
  #endif
#endif

// Code generation optimization level for the compiler
llvm::CodeGenOpt::Level Compiler::CodeGenOptLevel;

std::string Compiler::Triple;
llvm::Triple::ArchType Compiler::ArchType;

std::string Compiler::CPU;

std::vector<std::string> Compiler::Features;


//////////////////////////////////////////////////////////////////////////////
// Compiler
//////////////////////////////////////////////////////////////////////////////

void Compiler::GlobalInitialization() {
  if (GlobalInitialized) {
    return;
  }

#if defined(PROVIDE_ARM_CODEGEN)
  LLVMInitializeARMAsmPrinter();
  LLVMInitializeARMTargetMC();
  LLVMInitializeARMTargetInfo();
  LLVMInitializeARMTarget();
#endif

#if defined(PROVIDE_MIPS_CODEGEN)
  LLVMInitializeMipsAsmPrinter();
  LLVMInitializeMipsTargetMC();
  LLVMInitializeMipsTargetInfo();
  LLVMInitializeMipsTarget();
#endif

#if defined(PROVIDE_X86_CODEGEN)
  LLVMInitializeX86AsmPrinter();
  LLVMInitializeX86TargetMC();
  LLVMInitializeX86TargetInfo();
  LLVMInitializeX86Target();
#endif

#if USE_DISASSEMBLER
  InitializeDisassembler();
#endif

  // if (!llvm::llvm_is_multithreaded())
  //   llvm::llvm_start_multithreaded();

  // Set Triple, CPU and Features here
  Triple = TARGET_TRIPLE_STRING;

  // Determine ArchType
#if defined(__HOST__)
  {
    std::string Err;
    llvm::Target const *Target = llvm::TargetRegistry::lookupTarget(Triple, Err);
    if (Target != NULL) {
      ArchType = llvm::Triple::getArchTypeForLLVMName(Target->getName());
    } else {
      ArchType = llvm::Triple::UnknownArch;
      ALOGE("%s", Err.c_str());
    }
  }
#elif defined(DEFAULT_ARM_CODEGEN)
  ArchType = llvm::Triple::arm;
#elif defined(DEFAULT_MIPS_CODEGEN)
  ArchType = llvm::Triple::mipsel;
#elif defined(DEFAULT_X86_CODEGEN)
  ArchType = llvm::Triple::x86;
#elif defined(DEFAULT_X86_64_CODEGEN)
  ArchType = llvm::Triple::x86_64;
#else
  ArchType = llvm::Triple::UnknownArch;
#endif
#if defined(DEFAULT_ARM_CODEGEN) && defined(QCOM_LLVM)
#if defined(ARCH_ARM_MCPU_8660) || defined(ARCH_ARM_MCPU_8X55)
  CPU = "scorpion";
#endif
#if defined(ARCH_ARM_MCPU_8960) || defined (ARCH_ARM_MCPU_8064)
  CPU = "krait2";
#endif
#endif
ALOGE("CPU is %s", CPU.c_str() );
  if ((ArchType == llvm::Triple::arm) || (ArchType == llvm::Triple::thumb)) {
#  if defined(ARCH_ARM_HAVE_VFP)
    Features.push_back("+vfp3");
#  if !defined(ARCH_ARM_HAVE_VFP_D32)
    Features.push_back("+d16");
#  endif
#  endif

#  if defined(ARCH_ARM_HAVE_NEON) && !defined(DISABLE_ARCH_ARM_HAVE_NEON)
    Features.push_back("+neon");
    Features.push_back("+neonfp");
#  else
    Features.push_back("-neon");
    Features.push_back("-neonfp");
#  endif
  }

// set optimization flags
#if defined(QCOM_LLVM)
#if  defined(ARCH_ARM_MCPU_8960) || defined(ARCH_ARM_MCPU_8660) ||\
  defined(ARCH_ARM_MCPU_8X55) || defined(ARCH_ARM_MCPU_7X27A) || \
  defined(ARCH_ARM_MCPU_8064)
  // -mllvm -enable-rs-opt: -expand-limit=0 -check-vmlx-hazard=false
  // -unroll-threshold=1000 -unroll-allow-partial -pre-RA-sched=list-ilp
  std::vector<const char *> Opts;
  Opts.push_back("clang (LLVM option parsing)"); // Fake program name.
  Opts.push_back("-pre-RA-sched=list-ilp");
  Opts.push_back("-expand-limit=0");
  Opts.push_back("-check-vmlx-hazard=false");
  Opts.push_back(0);
  llvm::cl::ParseCommandLineOptions(Opts.size() - 1, const_cast<char **>(&Opts[0]));
#endif
#endif

  // Register the scheduler
  llvm::RegisterScheduler::setDefault(llvm::createDefaultScheduler);

  // Read in SHA1 checksum of libbcc and libRS.
  readSHA1(sha1LibBCC_SHA1, sizeof(sha1LibBCC_SHA1), pathLibBCC_SHA1);

  calcFileSHA1(sha1LibRS, pathLibRS);

  GlobalInitialized = true;
}


void Compiler::LLVMErrorHandler(void *UserData, const std::string &Message) {
  std::string *Error = static_cast<std::string*>(UserData);
  Error->assign(Message);
  ALOGE("%s", Message.c_str());
  exit(1);
}


Compiler::Compiler(ScriptCompiled *result)
  : mpResult(result),
    mRSExecutable(NULL),
    mpSymbolLookupFn(NULL),
    mpSymbolLookupContext(NULL),
    mModule(NULL),
    mHasLinked(false) /* Turn off linker */ {
  llvm::remove_fatal_error_handler();
  llvm::install_fatal_error_handler(LLVMErrorHandler, &mError);
  return;
}


int Compiler::linkModule(llvm::Module *moduleWith) {
  if (llvm::Linker::LinkModules(mModule, moduleWith,
                                llvm::Linker::PreserveSource,
                                &mError) != 0) {
    return hasError();
  }

  // Everything for linking should be settled down here with no error occurs
  mHasLinked = true;
  return hasError();
}


int Compiler::compile(const CompilerOption &option) {
  llvm::Target const *Target = NULL;
  llvm::TargetData *TD = NULL;
  llvm::TargetMachine *TM = NULL;

  std::vector<std::string> ExtraFeatures;

  std::string FeaturesStr;

  if (mModule == NULL)  // No module was loaded
    return 0;

  bcinfo::MetadataExtractor ME(mModule);
  ME.extract();

  size_t VarCount = ME.getExportVarCount();
  size_t FuncCount = ME.getExportFuncCount();
  size_t ForEachSigCount = ME.getExportForEachSignatureCount();
  size_t ObjectSlotCount = ME.getObjectSlotCount();
  size_t PragmaCount = ME.getPragmaCount();

  std::vector<std::string> &VarNameList = mpResult->mExportVarsName;
  std::vector<std::string> &FuncNameList = mpResult->mExportFuncsName;
  std::vector<std::string> &ForEachExpandList = mpResult->mExportForEachName;
  std::vector<std::string> ForEachNameList;
  std::vector<uint32_t> ForEachSigList;
  std::vector<const char*> ExportSymbols;

  // Defaults to maximum optimization level from MetadataExtractor.
  uint32_t OptimizationLevel = ME.getOptimizationLevel();

  if (OptimizationLevel == 0) {
    CodeGenOptLevel = llvm::CodeGenOpt::None;
  } else if (OptimizationLevel == 1) {
    CodeGenOptLevel = llvm::CodeGenOpt::Less;
  } else if (OptimizationLevel == 2) {
    CodeGenOptLevel = llvm::CodeGenOpt::Default;
  } else if (OptimizationLevel == 3) {
    CodeGenOptLevel = llvm::CodeGenOpt::Aggressive;
  }

  // not the best place for this, but we need to set the register allocation
  // policy after we read the optimization_level metadata from the bitcode

  // Register allocation policy:
  //  createFastRegisterAllocator: fast but bad quality
  //  createLinearScanRegisterAllocator: not so fast but good quality
  llvm::RegisterRegAlloc::setDefault
    ((CodeGenOptLevel == llvm::CodeGenOpt::None) ?
     llvm::createFastRegisterAllocator :
     llvm::createGreedyRegisterAllocator);

  // Find LLVM Target
  Target = llvm::TargetRegistry::lookupTarget(Triple, mError);
  if (hasError())
    goto on_bcc_compile_error;

#if defined(ARCH_ARM_HAVE_NEON)
  // Full-precision means we have to disable NEON
  if (ME.getRSFloatPrecision() == bcinfo::RS_FP_Full) {
    ExtraFeatures.push_back("-neon");
    ExtraFeatures.push_back("-neonfp");
  }
#endif

  if (!CPU.empty() || !Features.empty() || !ExtraFeatures.empty()) {
    llvm::SubtargetFeatures F;

    for (std::vector<std::string>::const_iterator
         I = Features.begin(), E = Features.end(); I != E; I++) {
      F.AddFeature(*I);
    }

    for (std::vector<std::string>::const_iterator
         I = ExtraFeatures.begin(), E = ExtraFeatures.end(); I != E; I++) {
      F.AddFeature(*I);
    }

    FeaturesStr = F.getString();
  }

  // Create LLVM Target Machine
  TM = Target->createTargetMachine(Triple, CPU, FeaturesStr,
                                   option.TargetOpt,
                                   option.RelocModelOpt,
                                   option.CodeModelOpt);

  if (TM == NULL) {
    setError("Failed to create target machine implementation for the"
             " specified triple '" + Triple + "'");
    goto on_bcc_compile_error;
  }

  // Get target data from Module
  TD = new llvm::TargetData(mModule);

  // Read pragma information from MetadataExtractor
  if (PragmaCount) {
    ScriptCompiled::PragmaList &PragmaPairs = mpResult->mPragmas;
    const char **PragmaKeys = ME.getPragmaKeyList();
    const char **PragmaValues = ME.getPragmaValueList();
    for (size_t i = 0; i < PragmaCount; i++) {
      PragmaPairs.push_back(std::make_pair(PragmaKeys[i], PragmaValues[i]));
    }
  }

  if (VarCount) {
    const char **VarNames = ME.getExportVarNameList();
    for (size_t i = 0; i < VarCount; i++) {
      VarNameList.push_back(VarNames[i]);
      ExportSymbols.push_back(VarNames[i]);
    }
  }

  if (FuncCount) {
    const char **FuncNames = ME.getExportFuncNameList();
    for (size_t i = 0; i < FuncCount; i++) {
      FuncNameList.push_back(FuncNames[i]);
      ExportSymbols.push_back(FuncNames[i]);
    }
  }

  if (ForEachSigCount) {
    const char **ForEachNames = ME.getExportForEachNameList();
    const uint32_t *ForEachSigs = ME.getExportForEachSignatureList();
    for (size_t i = 0; i < ForEachSigCount; i++) {
      std::string Name(ForEachNames[i]);
      ForEachNameList.push_back(Name);
      ForEachExpandList.push_back(Name + ".expand");
      ForEachSigList.push_back(ForEachSigs[i]);
    }

    // Need to wait until ForEachExpandList is fully populated to fill in
    // exported symbols.
    for (size_t i = 0; i < ForEachSigCount; i++) {
      ExportSymbols.push_back(ForEachExpandList[i].c_str());
    }
  }

  if (ObjectSlotCount) {
    ScriptCompiled::ObjectSlotList &objectSlotList = mpResult->mObjectSlots;
    const uint32_t *ObjectSlots = ME.getObjectSlotList();
    for (size_t i = 0; i < ObjectSlotCount; i++) {
      objectSlotList.push_back(ObjectSlots[i]);
    }
  }

  runInternalPasses(ForEachNameList, ForEachSigList);

  // Perform link-time optimization if we have multiple modules
  if (mHasLinked) {
    runLTO(new llvm::TargetData(*TD), ExportSymbols, CodeGenOptLevel);
  }

  // Perform code generation
  if (runMCCodeGen(new llvm::TargetData(*TD), TM) != 0) {
    goto on_bcc_compile_error;
  }

  if (!option.LoadAfterCompile)
    return 0;

  // Load the ELF Object
  mRSExecutable =
      rsloaderCreateExec((unsigned char *)&*mEmittedELFExecutable.begin(),
                         mEmittedELFExecutable.size(),
                         &resolveSymbolAdapter, this);

  if (!mRSExecutable) {
    setError("Fail to load emitted ELF relocatable file");
    goto on_bcc_compile_error;
  }

  rsloaderUpdateSectionHeaders(mRSExecutable,
      (unsigned char*) mEmittedELFExecutable.begin());

  // Once the ELF object has been loaded, populate the various slots for RS
  // with the appropriate relocated addresses.
  if (VarCount) {
    ScriptCompiled::ExportVarList &VarList = mpResult->mExportVars;
    for (size_t i = 0; i < VarCount; i++) {
      VarList.push_back(rsloaderGetSymbolAddress(mRSExecutable,
                                                 VarNameList[i].c_str()));
    }
  }

  if (FuncCount) {
    ScriptCompiled::ExportFuncList &FuncList = mpResult->mExportFuncs;
    for (size_t i = 0; i < FuncCount; i++) {
      FuncList.push_back(rsloaderGetSymbolAddress(mRSExecutable,
                                                  FuncNameList[i].c_str()));
    }
  }

  if (ForEachSigCount) {
    ScriptCompiled::ExportForEachList &ForEachList = mpResult->mExportForEach;
    for (size_t i = 0; i < ForEachSigCount; i++) {
      ForEachList.push_back(rsloaderGetSymbolAddress(mRSExecutable,
          ForEachExpandList[i].c_str()));
    }
  }

#if DEBUG_MC_DISASSEMBLER
  {
    // Get MC codegen emitted function name list
    size_t func_list_size = rsloaderGetFuncCount(mRSExecutable);
    std::vector<char const *> func_list(func_list_size, NULL);
    rsloaderGetFuncNameList(mRSExecutable, func_list_size, &*func_list.begin());

    // Disassemble each function
    for (size_t i = 0; i < func_list_size; ++i) {
      void *func = rsloaderGetSymbolAddress(mRSExecutable, func_list[i]);
      if (func) {
        size_t size = rsloaderGetSymbolSize(mRSExecutable, func_list[i]);
        Disassemble(DEBUG_MC_DISASSEMBLER_FILE,
                    Target, TM, func_list[i], (unsigned char const *)func, size);
      }
    }
  }
#endif

on_bcc_compile_error:
  // ALOGE("on_bcc_compiler_error");
  if (TD) {
    delete TD;
  }

  if (TM) {
    delete TM;
  }

  if (mError.empty()) {
    return 0;
  }

  // ALOGE(getErrorMessage());
  return 1;
}


int Compiler::runMCCodeGen(llvm::TargetData *TD, llvm::TargetMachine *TM) {
  // Decorate mEmittedELFExecutable with formatted ostream
  llvm::raw_svector_ostream OutSVOS(mEmittedELFExecutable);

  // Relax all machine instructions
  TM->setMCRelaxAll(/* RelaxAll= */ true);

  // Create MC code generation pass manager
  llvm::PassManager MCCodeGenPasses;

  // Add TargetData to MC code generation pass manager
  MCCodeGenPasses.add(TD);

  // Add MC code generation passes to pass manager
  llvm::MCContext *Ctx = NULL;
  if (TM->addPassesToEmitMC(MCCodeGenPasses, Ctx, OutSVOS, false)) {
    setError("Fail to add passes to emit file");
    return 1;
  }

  MCCodeGenPasses.run(*mModule);
  OutSVOS.flush();
  return 0;
}

int Compiler::runInternalPasses(std::vector<std::string>& Names,
                                std::vector<uint32_t>& Signatures) {
  llvm::PassManager BCCPasses;

  // Expand ForEach on CPU path to reduce launch overhead.
  BCCPasses.add(createForEachExpandPass(Names, Signatures));

  BCCPasses.run(*mModule);

  return 0;
}

int Compiler::runLTO(llvm::TargetData *TD,
                     std::vector<const char*>& ExportSymbols,
                     llvm::CodeGenOpt::Level OptimizationLevel) {
  // Note: ExportSymbols is a workaround for getting all exported variable,
  // function, and kernel names.
  // We should refine it soon.

  // TODO(logan): Remove this after we have finished the
  // bccMarkExternalSymbol API.

  // root(), init(), and .rs.dtor() are born to be exported
  ExportSymbols.push_back("root");
  ExportSymbols.push_back("init");
  ExportSymbols.push_back(".rs.dtor");

  // User-defined exporting symbols
  std::vector<char const *> const &UserDefinedExternalSymbols =
    mpResult->getUserDefinedExternalSymbols();

  std::copy(UserDefinedExternalSymbols.begin(),
            UserDefinedExternalSymbols.end(),
            std::back_inserter(ExportSymbols));

  llvm::PassManager LTOPasses;

  // Add TargetData to LTO passes
  LTOPasses.add(TD);

  // We now create passes list performing LTO. These are copied from
  // (including comments) llvm::createStandardLTOPasses().
  // Only a subset of these LTO passes are enabled in optimization level 0
  // as they interfere with interactive debugging.
  // FIXME: figure out which passes (if any) makes sense for levels 1 and 2

  if (OptimizationLevel != llvm::CodeGenOpt::None) {
    // Internalize all other symbols not listed in ExportSymbols
    LTOPasses.add(llvm::createInternalizePass(ExportSymbols));

    // Propagate constants at call sites into the functions they call. This
    // opens opportunities for globalopt (and inlining) by substituting
    // function pointers passed as arguments to direct uses of functions.
    LTOPasses.add(llvm::createIPSCCPPass());

    // Now that we internalized some globals, see if we can hack on them!
    LTOPasses.add(llvm::createGlobalOptimizerPass());

    // Linking modules together can lead to duplicated global constants, only
    // keep one copy of each constant...
    LTOPasses.add(llvm::createConstantMergePass());

    // Remove unused arguments from functions...
    LTOPasses.add(llvm::createDeadArgEliminationPass());

    // Reduce the code after globalopt and ipsccp. Both can open up
    // significant simplification opportunities, and both can propagate
    // functions through function pointers. When this happens, we often have
    // to resolve varargs calls, etc, so let instcombine do this.
    LTOPasses.add(llvm::createInstructionCombiningPass());

    // Inline small functions
    LTOPasses.add(llvm::createFunctionInliningPass());

    // Remove dead EH info.
    LTOPasses.add(llvm::createPruneEHPass());

    // Internalize the globals again after inlining
    LTOPasses.add(llvm::createGlobalOptimizerPass());

    // Remove dead functions.
    LTOPasses.add(llvm::createGlobalDCEPass());

    // If we didn't decide to inline a function, check to see if we can
    // transform it to pass arguments by value instead of by reference.
    LTOPasses.add(llvm::createArgumentPromotionPass());

    // The IPO passes may leave cruft around.  Clean up after them.
    LTOPasses.add(llvm::createInstructionCombiningPass());
    LTOPasses.add(llvm::createJumpThreadingPass());

    // Break up allocas
    LTOPasses.add(llvm::createScalarReplAggregatesPass());

    // Run a few AA driven optimizations here and now, to cleanup the code.
    LTOPasses.add(llvm::createFunctionAttrsPass());  // Add nocapture.
    LTOPasses.add(llvm::createGlobalsModRefPass());  // IP alias analysis.

    // Hoist loop invariants.
    LTOPasses.add(llvm::createLICMPass());

    // Remove redundancies.
    LTOPasses.add(llvm::createGVNPass());

    // Remove dead memcpys.
    LTOPasses.add(llvm::createMemCpyOptPass());

    // Nuke dead stores.
    LTOPasses.add(llvm::createDeadStoreEliminationPass());

    // Cleanup and simplify the code after the scalar optimizations.
    LTOPasses.add(llvm::createInstructionCombiningPass());

    LTOPasses.add(llvm::createJumpThreadingPass());

    // Delete basic blocks, which optimization passes may have killed.
    LTOPasses.add(llvm::createCFGSimplificationPass());

    // Now that we have optimized the program, discard unreachable functions.
    LTOPasses.add(llvm::createGlobalDCEPass());

  } else {
    LTOPasses.add(llvm::createInternalizePass(ExportSymbols));
    LTOPasses.add(llvm::createGlobalOptimizerPass());
    LTOPasses.add(llvm::createConstantMergePass());
  }

  LTOPasses.run(*mModule);

#if ANDROID_ENGINEERING_BUILD
  if (0 != gDebugDumpDirectory) {
    std::string errs;
    std::string Filename(gDebugDumpDirectory);
    Filename += "/post-lto-module.ll";
    llvm::raw_fd_ostream FS(Filename.c_str(), errs);
    mModule->print(FS, 0);
    FS.close();
  }
#endif

  return 0;
}


void *Compiler::getSymbolAddress(char const *name) {
  return rsloaderGetSymbolAddress(mRSExecutable, name);
}


void *Compiler::resolveSymbolAdapter(void *context, char const *name) {
  Compiler *self = reinterpret_cast<Compiler *>(context);

  if (void *Addr = FindRuntimeFunction(name)) {
    return Addr;
  }

  if (self->mpSymbolLookupFn) {
    if (void *Addr = self->mpSymbolLookupFn(self->mpSymbolLookupContext, name)) {
      return Addr;
    }
  }

  ALOGE("Unable to resolve symbol: %s\n", name);
  return NULL;
}


Compiler::~Compiler() {
  rsloaderDisposeExec(mRSExecutable);

  // llvm::llvm_shutdown();
}


}  // namespace bcc

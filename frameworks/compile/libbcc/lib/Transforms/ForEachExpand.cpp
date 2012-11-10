/*
 * Copyright 2012, The Android Open Source Project
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
#include "bcc/bcc_assert.h"

#include "DebugHelper.h"

#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Type.h"
#include "llvm/Support/IRBuilder.h"

namespace {
  /* ForEachExpandPass - This pass operates on functions that are able to be
   * called via rsForEach() or "foreach_<NAME>". We create an inner loop for
   * the ForEach-able function to be invoked over the appropriate data cells
   * of the input/output allocations (adjusting other relevant parameters as
   * we go). We support doing this for any ForEach-able compute kernels.
   * The new function name is the original function name followed by
   * ".expand". Note that we still generate code for the original function.
   */
  class ForEachExpandPass : public llvm::ModulePass {
  private:
  static char ID;

  llvm::Module *M;
  llvm::LLVMContext *C;

  std::vector<std::string>& mNames;
  std::vector<uint32_t>& mSignatures;

  uint32_t getRootSignature(llvm::Function *F) {
    const llvm::NamedMDNode *ExportForEachMetadata =
        M->getNamedMetadata("#rs_export_foreach");

    if (!ExportForEachMetadata) {
      llvm::SmallVector<llvm::Type*, 8> RootArgTys;
      for (llvm::Function::arg_iterator B = F->arg_begin(),
                                        E = F->arg_end();
           B != E;
           ++B) {
        RootArgTys.push_back(B->getType());
      }

      // For pre-ICS bitcode, we may not have signature information. In that
      // case, we use the size of the RootArgTys to select the number of
      // arguments.
      return (1 << RootArgTys.size()) - 1;
    }

    bccAssert(ExportForEachMetadata->getNumOperands() > 0);

    // We only handle the case for legacy root() functions here, so this is
    // hard-coded to look at only the first such function.
    llvm::MDNode *SigNode = ExportForEachMetadata->getOperand(0);
    if (SigNode != NULL && SigNode->getNumOperands() == 1) {
      llvm::Value *SigVal = SigNode->getOperand(0);
      if (SigVal->getValueID() == llvm::Value::MDStringVal) {
        llvm::StringRef SigString =
            static_cast<llvm::MDString*>(SigVal)->getString();
        uint32_t Signature = 0;
        if (SigString.getAsInteger(10, Signature)) {
          ALOGE("Non-integer signature value '%s'", SigString.str().c_str());
          return 0;
        }
        return Signature;
      }
    }

    return 0;
  }

  static bool hasIn(uint32_t Signature) {
    return Signature & 1;
  }

  static bool hasOut(uint32_t Signature) {
    return Signature & 2;
  }

  static bool hasUsrData(uint32_t Signature) {
    return Signature & 4;
  }

  static bool hasX(uint32_t Signature) {
    return Signature & 8;
  }

  static bool hasY(uint32_t Signature) {
    return Signature & 16;
  }

  public:
  ForEachExpandPass(std::vector<std::string>& Names,
                    std::vector<uint32_t>& Signatures)
      : ModulePass(ID), M(NULL), C(NULL), mNames(Names),
        mSignatures(Signatures) {
  }

  /* Performs the actual optimization on a selected function. On success, the
   * Module will contain a new function of the name "<NAME>.expand" that
   * invokes <NAME>() in a loop with the appropriate parameters.
   */
  bool ExpandFunction(llvm::Function *F, uint32_t Signature) {
    ALOGV("Expanding ForEach-able Function %s", F->getName().str().c_str());

    if (!Signature) {
      Signature = getRootSignature(F);
      if (!Signature) {
        // We couldn't determine how to expand this function based on its
        // function signature.
        return false;
      }
    }

    llvm::Type *VoidPtrTy = llvm::Type::getInt8PtrTy(*C);
    llvm::Type *Int32Ty = llvm::Type::getInt32Ty(*C);
    llvm::Type *SizeTy = Int32Ty;

    /* Defined in frameworks/base/libs/rs/rs_hal.h:
     *
     * struct RsForEachStubParamStruct {
     *   const void *in;
     *   void *out;
     *   const void *usr;
     *   size_t usr_len;
     *   uint32_t x;
     *   uint32_t y;
     *   uint32_t z;
     *   uint32_t lod;
     *   enum RsAllocationCubemapFace face;
     *   uint32_t ar[16];
     * };
     */
    llvm::SmallVector<llvm::Type*, 9> StructTys;
    StructTys.push_back(VoidPtrTy);  // const void *in
    StructTys.push_back(VoidPtrTy);  // void *out
    StructTys.push_back(VoidPtrTy);  // const void *usr
    StructTys.push_back(SizeTy);     // size_t usr_len
    StructTys.push_back(Int32Ty);    // uint32_t x
    StructTys.push_back(Int32Ty);    // uint32_t y
    StructTys.push_back(Int32Ty);    // uint32_t z
    StructTys.push_back(Int32Ty);    // uint32_t lod
    StructTys.push_back(Int32Ty);    // enum RsAllocationCubemapFace
    StructTys.push_back(llvm::ArrayType::get(Int32Ty, 16));  // uint32_t ar[16]

    llvm::Type *ForEachStubPtrTy = llvm::StructType::create(
        StructTys, "RsForEachStubParamStruct")->getPointerTo();

    /* Create the function signature for our expanded function.
     * void (const RsForEachStubParamStruct *p, uint32_t x1, uint32_t x2,
     *       uint32_t instep, uint32_t outstep)
     */
    llvm::SmallVector<llvm::Type*, 8> ParamTys;
    ParamTys.push_back(ForEachStubPtrTy);  // const RsForEachStubParamStruct *p
    ParamTys.push_back(Int32Ty);           // uint32_t x1
    ParamTys.push_back(Int32Ty);           // uint32_t x2
    ParamTys.push_back(Int32Ty);           // uint32_t instep
    ParamTys.push_back(Int32Ty);           // uint32_t outstep

    llvm::FunctionType *FT =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*C), ParamTys, false);
    llvm::Function *ExpandedFunc =
        llvm::Function::Create(FT,
                               llvm::GlobalValue::ExternalLinkage,
                               F->getName() + ".expand", M);

    // Create and name the actual arguments to this expanded function.
    llvm::SmallVector<llvm::Argument*, 8> ArgVec;
    for (llvm::Function::arg_iterator B = ExpandedFunc->arg_begin(),
                                      E = ExpandedFunc->arg_end();
         B != E;
         ++B) {
      ArgVec.push_back(B);
    }

    if (ArgVec.size() != 5) {
      ALOGE("Incorrect number of arguments to function: %zu",
            ArgVec.size());
      return false;
    }
    llvm::Value *Arg_p = ArgVec[0];
    llvm::Value *Arg_x1 = ArgVec[1];
    llvm::Value *Arg_x2 = ArgVec[2];
    llvm::Value *Arg_instep = ArgVec[3];
    llvm::Value *Arg_outstep = ArgVec[4];

    Arg_p->setName("p");
    Arg_x1->setName("x1");
    Arg_x2->setName("x2");
    Arg_instep->setName("instep");
    Arg_outstep->setName("outstep");

    // Construct the actual function body.
    llvm::BasicBlock *Begin =
        llvm::BasicBlock::Create(*C, "Begin", ExpandedFunc);
    llvm::IRBuilder<> Builder(Begin);

    // uint32_t X = x1;
    llvm::AllocaInst *AX = Builder.CreateAlloca(Int32Ty, 0, "AX");
    Builder.CreateStore(Arg_x1, AX);

    // Collect and construct the arguments for the kernel().
    // Note that we load any loop-invariant arguments before entering the Loop.
    llvm::Function::arg_iterator Args = F->arg_begin();

    llvm::Type *InTy = NULL;
    llvm::AllocaInst *AIn = NULL;
    if (hasIn(Signature)) {
      InTy = Args->getType();
      AIn = Builder.CreateAlloca(InTy, 0, "AIn");
      Builder.CreateStore(Builder.CreatePointerCast(Builder.CreateLoad(
          Builder.CreateStructGEP(Arg_p, 0)), InTy), AIn);
      Args++;
    }

    llvm::Type *OutTy = NULL;
    llvm::AllocaInst *AOut = NULL;
    if (hasOut(Signature)) {
      OutTy = Args->getType();
      AOut = Builder.CreateAlloca(OutTy, 0, "AOut");
      Builder.CreateStore(Builder.CreatePointerCast(Builder.CreateLoad(
          Builder.CreateStructGEP(Arg_p, 1)), OutTy), AOut);
      Args++;
    }

    llvm::Value *UsrData = NULL;
    if (hasUsrData(Signature)) {
      llvm::Type *UsrDataTy = Args->getType();
      UsrData = Builder.CreatePointerCast(Builder.CreateLoad(
          Builder.CreateStructGEP(Arg_p, 2)), UsrDataTy);
      UsrData->setName("UsrData");
      Args++;
    }

    if (hasX(Signature)) {
      Args++;
    }

    llvm::Value *Y = NULL;
    if (hasY(Signature)) {
      Y = Builder.CreateLoad(Builder.CreateStructGEP(Arg_p, 5), "Y");
      Args++;
    }

    bccAssert(Args == F->arg_end());

    llvm::BasicBlock *Loop = llvm::BasicBlock::Create(*C, "Loop", ExpandedFunc);
    llvm::BasicBlock *Exit = llvm::BasicBlock::Create(*C, "Exit", ExpandedFunc);

    // if (x1 < x2) goto Loop; else goto Exit;
    llvm::Value *Cond = Builder.CreateICmpSLT(Arg_x1, Arg_x2);
    Builder.CreateCondBr(Cond, Loop, Exit);

    // Loop:
    Builder.SetInsertPoint(Loop);

    // Populate the actual call to kernel().
    llvm::SmallVector<llvm::Value*, 8> RootArgs;

    llvm::Value *In = NULL;
    llvm::Value *Out = NULL;

    if (AIn) {
      In = Builder.CreateLoad(AIn, "In");
      RootArgs.push_back(In);
    }

    if (AOut) {
      Out = Builder.CreateLoad(AOut, "Out");
      RootArgs.push_back(Out);
    }

    if (UsrData) {
      RootArgs.push_back(UsrData);
    }

    // We always have to load X, since it is used to iterate through the loop.
    llvm::Value *X = Builder.CreateLoad(AX, "X");
    if (hasX(Signature)) {
      RootArgs.push_back(X);
    }

    if (Y) {
      RootArgs.push_back(Y);
    }

    Builder.CreateCall(F, RootArgs);

    if (In) {
      // In += instep
      llvm::Value *NewIn = Builder.CreateIntToPtr(Builder.CreateNUWAdd(
          Builder.CreatePtrToInt(In, Int32Ty), Arg_instep), InTy);
      Builder.CreateStore(NewIn, AIn);
    }

    if (Out) {
      // Out += outstep
      llvm::Value *NewOut = Builder.CreateIntToPtr(Builder.CreateNUWAdd(
          Builder.CreatePtrToInt(Out, Int32Ty), Arg_outstep), OutTy);
      Builder.CreateStore(NewOut, AOut);
    }

    // X++;
    llvm::Value *XPlusOne =
        Builder.CreateNUWAdd(X, llvm::ConstantInt::get(Int32Ty, 1));
    Builder.CreateStore(XPlusOne, AX);

    // If (X < x2) goto Loop; else goto Exit;
    Cond = Builder.CreateICmpSLT(XPlusOne, Arg_x2);
    Builder.CreateCondBr(Cond, Loop, Exit);

    // Exit:
    Builder.SetInsertPoint(Exit);
    Builder.CreateRetVoid();

    return true;
  }

  virtual bool runOnModule(llvm::Module &M) {
    bool Changed = false;
    this->M = &M;
    C = &M.getContext();

    bccAssert(mNames.size() == mSignatures.size());
    for (int i = 0, e = mNames.size(); i != e; i++) {
      llvm::Function *kernel = M.getFunction(mNames[i]);
      if (kernel && kernel->getReturnType()->isVoidTy()) {
        Changed |= ExpandFunction(kernel, mSignatures[i]);
      }
    }

    return Changed;
  }

  virtual const char *getPassName() const {
    return "ForEach-able Function Expansion";
  }

  };
}  // end anonymous namespace

char ForEachExpandPass::ID = 0;

namespace bcc {

  llvm::ModulePass *createForEachExpandPass(std::vector<std::string>& Names,
                                            std::vector<uint32_t>& Signatures) {
    return new ForEachExpandPass(Names, Signatures);
  }

}  // namespace bcc

/*
 * Copyright 2011, The Android Open Source Project
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

#include "Disassembler.h"

#include "Config.h"

#include "DebugHelper.h"
#include "ExecutionEngine/Compiler.h"

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstPrinter.h"

#include "llvm/Support/MemoryObject.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"

#include "llvm/LLVMContext.h"

#if USE_DISASSEMBLER

namespace {

class BufferMemoryObject : public llvm::MemoryObject {
private:
  const uint8_t *mBytes;
  uint64_t mLength;

public:
  BufferMemoryObject(const uint8_t *Bytes, uint64_t Length)
    : mBytes(Bytes), mLength(Length) {
  }

  virtual uint64_t getBase() const { return 0; }
  virtual uint64_t getExtent() const { return mLength; }

  virtual int readByte(uint64_t Addr, uint8_t *Byte) const {
    if (Addr > getExtent())
      return -1;
    *Byte = mBytes[Addr];
    return 0;
  }
};

} // namespace anonymous

namespace bcc {

void InitializeDisassembler() {
#if defined(PROVIDE_ARM_CODEGEN)
  LLVMInitializeARMDisassembler();
#endif

#if defined(PROVIDE_X86_CODEGEN)
  LLVMInitializeX86Disassembler();
#endif
}

void Disassemble(char const *OutputFileName,
                 llvm::Target const *Target,
                 llvm::TargetMachine *TM,
                 std::string const &Name,
                 unsigned char const *Func,
                 size_t FuncSize) {

  std::string ErrorInfo;

  // Open the disassembler output file
  llvm::raw_fd_ostream OS(OutputFileName, ErrorInfo,
                          llvm::raw_fd_ostream::F_Append);

  if (!ErrorInfo.empty()) {
    ALOGE("Unable to open disassembler output file: %s\n", OutputFileName);
    return;
  }

  // Disassemble the given function
  OS << "Disassembled code: " << Name << "\n";

  const llvm::MCAsmInfo *AsmInfo;
  const llvm::MCSubtargetInfo *SubtargetInfo;
  const llvm::MCDisassembler *Disassmbler;
  llvm::MCInstPrinter *IP;

  AsmInfo = Target->createMCAsmInfo(Compiler::getTargetTriple());
  SubtargetInfo = Target->createMCSubtargetInfo(Compiler::getTargetTriple(), "", "");
  Disassmbler = Target->createMCDisassembler(*SubtargetInfo);
  IP = Target->createMCInstPrinter(AsmInfo->getAssemblerDialect(),
                                   *AsmInfo, *SubtargetInfo);

  const BufferMemoryObject *BufferMObj = new BufferMemoryObject(Func, FuncSize);

  uint64_t Size;
  uint64_t Index;

  for (Index = 0; Index < FuncSize; Index += Size) {
    llvm::MCInst Inst;

    if (Disassmbler->getInstruction(Inst, Size, *BufferMObj, Index,
                           /* REMOVED */ llvm::nulls(), llvm::nulls())) {
      OS.indent(4);
      OS.write("0x", 2);
      OS.write_hex((uint32_t)Func + Index);
      OS.write(": 0x", 4);
      OS.write_hex(*(uint32_t *)(Func + Index));
      IP->printInst(&Inst, OS, "");
      OS << "\n";
    } else {
      if (Size == 0)
        Size = 1;  // skip illegible bytes
    }
  }

  OS << "\n";

  delete BufferMObj;

  delete AsmInfo;
  delete Disassmbler;
  delete IP;

  OS.close();
}

} // namespace bcc

#endif // USE_DISASSEMBLER

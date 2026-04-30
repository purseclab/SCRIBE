#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include <iostream>
#include <fstream>
#include <string>
#include "ParseCmdOpts.h"

using namespace llvm;

namespace {

//===----------------------------------------------------------------------===//
// overrideStackOffset - Machine Function Pass
//===----------------------------------------------------------------------===//
struct overrideStackOffset : public MachineFunctionPass {
  static char ID;
  overrideStackOffset() : MachineFunctionPass(ID) {}

  void replaceFrameIdxWithOffset(MachineInstr &MI, unsigned operandIdx, Register bp, int offset) {
    MI.getOperand(operandIdx).ChangeToRegister(bp, false);
    if (MI.getOperand(operandIdx + 3).isImm()) {
      auto imm = MI.getOperand(operandIdx + 3).getImm();
      MI.getOperand(operandIdx + 3).setImm(offset + imm);
    }
      
  }

  void fixStackObject(MachineFrameInfo &MFI, int FrameIdx, int Offset, int Size) {
    // fixed stack objects so the prolog/epilog can be generated correctly
    bool Exists = false;
    for (int I = MFI.getObjectIndexBegin(); I < 0; ++I) {
      if (Offset == MFI.getObjectOffset(I)) {
        Exists = true;
        break;
      }
    }
    if (!Exists) {
      MFI.CreateFixedObject(Size, Offset, false);
      // FIXME: ideally we should use MFI.RemoveStackObject(FrameIdx), but segfault
      // TODO: wait do we really want to remove the normal stack object?
      // what's the exact difference between fixed and normal stack object?
      // MFI.setObjectSize(FrameIdx, 0);
      MFI.setObjectOffset(FrameIdx, Offset);
    }
    errs() << "\tFixed " << "(FrameIndex " << FrameIdx << ") to [Offset = " << Offset << ", Size = " << Size << "]\n";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
    // const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
    MachineFrameInfo &MFI = MF.getFrameInfo();
    auto StkLocMap = parseStkLoc(MF.getName());
    std::map<int, int> FrameIdxOffsetMap;
    std::map<int, int> FrameIdxSizeMap;

    //===----------------------------------------------------------------------===//
    // Precalculate top of the stack
    //===----------------------------------------------------------------------===//
    int smallest = 0;
    for (auto &stkloc : StkLocMap) {
      if (stkloc.getValue() < smallest)
        smallest = stkloc.getValue();
    }

    // MFI.setOffsetAdjustment(smallest);
    // MFI.setStackSize(0x400);


    //===----------------------------------------------------------------------===//
    // Override stack offset
    //===----------------------------------------------------------------------===//
    // TODO: move to fixedStack?
    for (int I = 0, E = MFI.getObjectIndexEnd(), ID = 0; I < E; ++I, ++ID) {
      if (MFI.isDeadObjectIndex(I))
        continue;
      for (const auto &VI : MF.getVariableDbgInfo()) {
        if (VI.Var && VI.Slot == ID && StkLocMap.count(VI.Var->getName())) {
          int Offset = StkLocMap[VI.Var->getName()];
          MFI.setObjectOffset(I, Offset);
        }
      }
    }

    // PEI::replaceFrameIndices() -> TRI.eliminateFrameIndex() -> TFI->getFrameIndexReference()
    for(auto &BB : MF) {
      for(auto I = BB.begin(); I != BB.end(); ++I) {
        MachineInstr &MI = *I;
        if (TII.isFrameInstr(*I))
          continue;
        for (unsigned i = 0, e = MI.getNumOperands(); i != e; ++i) {
          if (!MI.getOperand(i).isFI())
            continue;
          int FrameIdx = MI.getOperand(i).getIndex();
          Register bp = TRI.getFrameRegister(MF);
          int Offset, Size;

          if (FrameIdxOffsetMap.count(FrameIdx)) {
            // if we have already set the offset of this frame index, use it
            Offset = FrameIdxOffsetMap[FrameIdx];
            Size = FrameIdxSizeMap[FrameIdx];
            replaceFrameIdxWithOffset(MI, i, bp, Offset);
          } else {
            // otherwise, try to find the offset from input file, if not found, calculate the offset
            bool found = false;
            Size = MFI.getObjectSize(FrameIdx);
            for (const auto &VI : MF.getVariableDbgInfo()) {
              if (VI.Var && /* VI.Var->getArg() == 0 && */ VI.Slot == FrameIdx) {
                // found the corresponding debug info
                errs() << VI.Var->getName() << " ";

                if (StkLocMap.count(VI.Var->getName())) {
                  // found match in input file, use the offset from debug info
                  found = true;
                  Offset = StkLocMap[VI.Var->getName()];
                  errs() << "found, using offset in config " << StkLocMap[VI.Var->getName()] << "\n";
                  break;
                }
              }
            }
            // not found, use the calculated offset
            if (!found) {
              smallest -= Size;
              // ensure the offset is aligned, for example: movaps requires 16-byte alignment
              smallest = -llvm::alignTo(-smallest, MFI.getObjectAlign(FrameIdx));
              Offset = smallest;
              errs() << "not found, using calculated offset " << smallest << "\n";
            }

            // update the offset of this frame index
            FrameIdxOffsetMap[FrameIdx] = Offset;
            FrameIdxSizeMap[FrameIdx] = Size;
            fixStackObject(MFI, FrameIdx, Offset, Size);
            replaceFrameIdxWithOffset(MI, i, bp, Offset);
          }
        }
      }
    }
    // iterate FrameIdxOffsetMap, FrameIdxSizeMap to check for overlap
    for (auto &i : FrameIdxOffsetMap) {
      int FrameIdx = i.first;
      int Offset = FrameIdxOffsetMap[FrameIdx];
      int Size = FrameIdxSizeMap[FrameIdx];

      // check for overlap
      for (auto &j : FrameIdxOffsetMap) {
        if (j.first == FrameIdx)
          continue;
        int Offset2 = FrameIdxOffsetMap[j.first];
        int Size2 = FrameIdxSizeMap[j.first];
        if (Offset == Offset2 && Size == Size2)
          continue; // same stack object, just different name
        if (Offset2 + Size2 > Offset && Offset2 < Offset + Size) {
          errs() << "Overlap detected: " << Offset << " " << Size << " " << Offset2 << " " << Size2 << "\n";
        }
      }
    }
    return true;
  }
};

} // end anonymous namespace

char overrideStackOffset::ID = 0;
static RegisterPass<overrideStackOffset> X("override-stack-offset", "Override stack offset of local variables");


/*
Notes, Don't delete this note unless you saved it somewhere else


System V AMD64 ABI

    # Section 3.2.2 The Stack Frame
    In addition to registers, each function has a frame on the run-time stack. This stack
    grows downwards from high addresses. Figure 3.3 shows the stack organization.
    The end of the input argument area shall be aligned on a 16 (32, if __m256 is
    passed on stack) byte boundary. In other words, the value (%rsp + 8) is always
    a multiple of 16 (32) when control is transferred to the function entry point. The
    stack pointer, %rsp, always points to the end of the latest allocated stack frame. 7
    The 128-byte area beyond the location pointed to by %rsp is considered to
    be reserved and shall not be modified by signal or interrupt handlers.8 Therefore,
    functions may use this area for temporary data that is not needed across function
    calls. In particular, leaf functions may use this area for their entire stack frame,
    rather than adjusting the stack pointer in the prologue and epilogue. This area is
    known as the red zone.

    func1 (misalign 8) -> func2 (misalign 8) -> malloc()

    ^^^ Align stack to 16 (or 32 if __m256 is passed on stack), which means sub rsp, 0XXX8h

Intel® 64 and IA-32 Architectures Software Developer’s Manual Combined Volumes: 1, 2A, 2B, 2C, 2D, 3A, 3B, 3C, 3D, and 4

    # Section 10.4.1.1 Intel® SSE Data Movement Instructions
    The MOVAPS (move aligned packed single precision floating-point values) instruction transfers a double quadword
    operand containing four packed single precision floating-point values from memory to an XMM register and vice
    versa, or between XMM registers. The memory address must be aligned to a 16-byte boundary; otherwise, a
    general-protection exception (#GP) is generated.

    ^^^ movaps memory address must be aligned to a 16-byte boundary
*/

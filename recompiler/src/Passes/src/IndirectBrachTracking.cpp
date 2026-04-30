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
struct IndirectBranchTracking : public MachineFunctionPass {
  static char ID;
  IndirectBranchTracking() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    auto &SubTarget = MF.getSubtarget();
    auto Triple = SubTarget.getTargetTriple();
    if (Triple.getArch() != Triple::x86_64 && Triple.getArch() != Triple::x86) {
        errs() << "IndirectBranchTracking: Unsupported architecture: " << Triple.getArchName() << "\n";
        return false;
    }
    errs() << "IndirectBranchTracking: " << MF.getName() << "\n";
    // get the target instruction info
    auto *TII = SubTarget.getInstrInfo();

    /*
    
    !!!!!!!!!!!!!!!!!!!!!!!!!!!
    just use -fcf-protection=branch, no need to do anything here
    !!!!!!!!!!!!!!!!!!!!!!!!!!!
    
    
    */


    // unsigned int EndbrOpcode = 
    // auto MBB = MF.begin();
    // auto I = MBB->begin();
    
    // // If the MBB/I is empty or the current instruction is not ENDBR,
    // // insert ENDBR instruction to the location of I.
    // if (I == MBB.end() || I->getOpcode() != EndbrOpcode) {
    //     BuildMI(MBB, I, MBB.findDebugLoc(I), TII->get(EndbrOpcode));
    //     ++NumEndBranchAdded;
    //     return true;
    // }
    return false;
  }
};

} // end anonymous namespace

char IndirectBranchTracking::ID = 0;
static RegisterPass<IndirectBranchTracking> X("indirect-branch-tracking", "TODO");

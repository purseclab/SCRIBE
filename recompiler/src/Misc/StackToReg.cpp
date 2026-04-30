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

using namespace llvm;

//===----------------------------------------------------------------------===//
// forceStackToRegister
//===----------------------------------------------------------------------===//
namespace {

  DILocalVariable *getLocalVariable(Value *V) {
    if (!V->isUsedByMetadata())
      return nullptr;
    auto *L = LocalAsMetadata::getIfExists(V);
    if (!L)
      return nullptr;
    auto *MDV = MetadataAsValue::getIfExists(V->getContext(), L);
    if (!MDV)
      return nullptr;
    
    for (User *U : MDV->users()) {
      if (auto *DII = dyn_cast<DbgVariableIntrinsic>(U))
        if (DII->isAddressOfVariable())
          return DII->getVariable();
    }
    return nullptr;
  }

  bool isAllocaInRegister(AllocaInst *AI) {
    // TODO: put reg info in debug info
    // TODO: check debug value if it's in register
    if (!getLocalVariable(AI)) {
      return false;
    }
    // if (getLocalVariable(AI)->getAMPDebug().StkLoc != 0) {
    //   errs() << "Found AMPDebug for Alloca - " << getLocalVariable(AI)->getAMPDebug().StkLoc << '\n';
    // }
    // if (getLocalVariable(AI)->getName() == "v4") {
    //   errs() << "Found v4 for Alloca - " << getLocalVariable(AI)->getName() << '\n';
    //   return true;
    // }
    return false;
  }

  void MoveAllocaToRegister(std::vector<AllocaInst *> &Allocas) {
    // TODO: see README.md
    for (AllocaInst *AI : Allocas) {
      for (auto UI = AI->user_begin(), E = AI->user_end(); UI != E;) {
        Instruction *User = cast<Instruction>(*UI++);

        if (dyn_cast<StoreInst>(User)) {
          // TODO: do we really have such cases?
        } else {
          // Otherwise it must be a load instruction
          LoadInst *LI = cast<LoadInst>(User);
          IRBuilder<> Builder(LI);
          InlineAsm *IA = InlineAsm::get(
            llvm::FunctionType::get(LI->getPointerOperandType(), false),
            "mov $0, rdx" /*instr*/, "=r" /*constraint*/, false /*side effects*/,
            false /*align stack*/, InlineAsm::AD_Intel);
          CallInst *Result = Builder.CreateCall(IA, {});
          LI->replaceAllUsesWith(Result);
          LI->eraseFromParent();
        }
      }
    }
  }

  struct forceStackToRegister : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    forceStackToRegister() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      errs() << "================== forceStackToRegister ==================\n";
      std::vector<AllocaInst *> Allocas;
      BasicBlock &BB = F.getEntryBlock();

      for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
        if (AllocaInst *AI = dyn_cast<AllocaInst>(I))
          if (isAllocaInRegister(AI))
            Allocas.push_back(AI);

      if (Allocas.empty())
        return false;

      MoveAllocaToRegister(Allocas);
      return true;
    }
  };
}

char forceStackToRegister::ID = 0;
static RegisterPass<forceStackToRegister> X1("force-stack-to-register", "TODO");

//===----------------------------------------------------------------------===//
// forceStackToRegisterMIR - Machine Function Pass
//===----------------------------------------------------------------------===//
namespace {

  DILocalVariable *getLocalVariableMIR(Value *V) {
    if (!V->isUsedByMetadata())
      return nullptr;
    auto *L = LocalAsMetadata::getIfExists(V);
    if (!L)
      return nullptr;
    auto *MDV = MetadataAsValue::getIfExists(V->getContext(), L);
    if (!MDV)
      return nullptr;
    
    for (User *U : MDV->users()) {
      if (auto *DII = dyn_cast<DbgVariableIntrinsic>(U))
        if (DII->isAddressOfVariable())
          return DII->getVariable();
    }
    return nullptr;
  }

  bool isAllocaInRegisterMIR(AllocaInst *AI) {
    // TODO: put reg info in debug info
    // TODO: check debug value if it's in register
    if (!getLocalVariable(AI)) {
      return false;
    }
    // if (getLocalVariable(AI)->getAMPDebug().StkLoc != 0) {
    //   errs() << "Found AMPDebug for Alloca - " << getLocalVariable(AI)->getAMPDebug().StkLoc << '\n';
    // }
    // if (getLocalVariable(AI)->getName() == "v4") {
    //   errs() << "Found v4 for Alloca - " << getLocalVariable(AI)->getName() << '\n';
    //   return true;
    // }
    return false;
  }
/*
  v1 // rdx
  from:  mov sth, [rbp + v1_off]
  to:    mov sth, rdx
*/
  void MoveToRegisterMIR(std::vector<AllocaInst *> &Allocas) {
    // TODO: see README.md
    for (AllocaInst *AI : Allocas) {
      for (auto UI = AI->user_begin(), E = AI->user_end(); UI != E;) {
        Instruction *User = cast<Instruction>(*UI++);

        if (dyn_cast<StoreInst>(User)) {
          // TODO: do we really have such cases?
        } else {
          // Otherwise it must be a load instruction
          LoadInst *LI = cast<LoadInst>(User);
          IRBuilder<> Builder(LI);
          InlineAsm *IA = InlineAsm::get(
            llvm::FunctionType::get(LI->getPointerOperandType(), false),
            "mov $0, rdx" /*instr*/, "=r" /*constraint*/, false /*side effects*/,
            false /*align stack*/, InlineAsm::AD_Intel);
          CallInst *Result = Builder.CreateCall(IA, {});
          LI->replaceAllUsesWith(Result);
          LI->eraseFromParent();
        }
      }
    }
  }

  struct forceStackToRegisterMIR : public MachineFunctionPass {
    static char ID; // Pass identification, replacement for typeid
    forceStackToRegisterMIR() : MachineFunctionPass(ID) {}

    bool runOnMachineFunction(MachineFunction &MF) override {
      errs() << "================== forceStackToRegisterMIR ==================\n";
      // for (const auto &VI : MF.getVariableDbgInfo()) {
      //   if (VI.Var) {
      //     errs() << "Found variable - " << VI.Var->getName() << '\n';
      //     if (VI.Var->getName() == "v4") {
            
      //     }
      //   }
      // }
      for (auto UI = MF.begin(), E = MF.end(); UI != E; ++UI)
        return true;
      return true;
    }
  };
}

char forceStackToRegisterMIR::ID = 0;
static RegisterPass<forceStackToRegisterMIR> X2("force-stack-to-register-mir", "TODO");


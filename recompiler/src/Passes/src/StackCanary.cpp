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
// EnableStackProtection
//===----------------------------------------------------------------------===//
namespace {
  struct EnableStackProtection : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    EnableStackProtection() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
        // check if there is a call to __readfsqword
        bool found = false;
        for (auto BB = F.begin(); BB != F.end(); ++BB) {
            for (auto I = BB->begin(); I != BB->end(); ++I) {
                if (auto *Call = dyn_cast<CallInst>(I)) {
                    Value *CalledValue = Call->getCalledOperand();
                    Value *StrippedValue = CalledValue->stripInBoundsConstantOffsets();

                    if (Function *Callee = dyn_cast<Function>(StrippedValue)) {
                        // errs() << "Callee: " << Callee->getName() << "\n";
                        // TODO: 1. __readfsqword is ida only 2. remove __readfsqword def in ida_extra.h
                        if (Callee->getName() == "__readfsqword") {
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        if (found) {
            errs () << "Found __readfsqword in " << F.getName() << "\n";
            F.addFnAttr(llvm::Attribute::StackProtectReq);
            return true;
        }
        errs() << "Did not find __readfsqword in " << F.getName() << "\n";
        return false;
    }
  };
}

char EnableStackProtection::ID = 0;
static RegisterPass<EnableStackProtection> X("enable-stack-protection", "TODO");


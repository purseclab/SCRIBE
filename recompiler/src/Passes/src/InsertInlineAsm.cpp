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
// InsertInlineAsm
//===----------------------------------------------------------------------===//
namespace {
  struct InsertInlineAsm : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    InsertInlineAsm() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      IRBuilder<> Builder(F.getContext());
      bool Changed = false;
      for (auto BB = F.begin(); BB != F.end(); ++BB) {
        for (auto I = BB->begin(); I != BB->end(); ++I) {
          if (auto *Call = dyn_cast<CallInst>(I)) {
            Function *Callee = Call->getCalledFunction();
            if (!Callee)
              continue;
            if (Callee->getName() == "printf") {
              // Prepare InlineAsm Args
              std::vector<Value *> Args;
              Args.push_back(Callee);
              for (auto &Arg : Call->args()) {
                Args.push_back(Arg);
              }

              // Prepare InlineAsm Prototype
              FunctionType *FT = Callee->getFunctionType();
              Type *RetTy = FT->getReturnType();
              std::vector<Type *> ArgTys = FT->params();
              ArgTys.insert(ArgTys.begin(), PointerType::getUnqual(Type::getInt8Ty(F.getContext())));
              FunctionType *NewFT = FunctionType::get(RetTy, ArgTys, false/*isVarArg*/);

              // Prepare InlineAsm
              // $0 = return value, $1 = function pointer, $2-$n = arguments
              std::string Assembly = "mov rdi, $2; mov rsi, $3; call $1; mov eax, $0";
              InlineAsm *InlineCode = InlineAsm::get(NewFT, Assembly, "=r,r,r,r,~{rdi},~{rsi},~{rax}", true, false, InlineAsm::AD_Intel);

              // Insert InlineAsm
              Builder.SetInsertPoint(Call);
              Value *Result = Builder.CreateCall(InlineCode, Args);
              Call->replaceAllUsesWith(Result);
              Call->eraseFromParent();
              Changed = true;

              // Re-iterate Over The Basic Block
              BB = F.begin();
              I = BB->begin();
              continue;
            }
          }
        }
      }
      return Changed;
    }
  };
}

char InsertInlineAsm::ID = 0;
static RegisterPass<InsertInlineAsm> X("insert-inline-asm", "TODO");


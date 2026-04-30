#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IntrinsicInst.h"
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

struct localVariableToArg : public FunctionPass {
    static char ID;
    localVariableToArg() : FunctionPass(ID) {}

    DILocalVariable *getLocalVariable(Value *V) {
        Metadata *L = nullptr;
        Value *MDV = nullptr;
        if (!V->isUsedByMetadata() || !(L = LocalAsMetadata::getIfExists(V)) || !(MDV = MetadataAsValue::getIfExists(V->getContext(), L)))
            return nullptr;
        
        for (User *U : MDV->users()) {
            if (auto *DII = dyn_cast<DbgVariableIntrinsic>(U))
                if (DII->isAddressOfVariable())
                    return DII->getVariable();
        }
        return nullptr;
    }

    bool runOnFunction(Function &F) override {

        std::vector<AllocaInst *> Allocas;
        BasicBlock &BB = F.getEntryBlock();
        auto StkLocMap = parseStkLoc(F.getName());

        int numArgs = 0;
        for (auto &Arg : F.args()){
            numArgs++;
        }
        errs() << "Number of args: " << numArgs << "\n";


        for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I){
            if (AllocaInst *AI = dyn_cast<AllocaInst>(I)){
                DILocalVariable *DIVar = getLocalVariable(AI);
                if (DIVar && StkLocMap.count(DIVar->getName().str())){
                    if (StkLocMap[DIVar->getName().str()] >= ((numArgs - 6 + 1) * 8)){
                        errs() << "Found local variable " << DIVar->getName().str() << " with offset " << StkLocMap[DIVar->getName().str()] << "\n";
                        errs() << "AllocaInst: " << *AI << "\n";
                        if (numArgs < 6){
                            // clone current function with new arg
                            std::vector<Type *> argTypes;
                            for (auto &Arg : F.args()){
                                argTypes.push_back(Arg.getType());
                            }
                            
                        }
                    }
                }
            }
        }

        return true;
    }


};

} // end anonymous namespace

char localVariableToArg::ID = 0;
static RegisterPass<localVariableToArg> X("local-var-to-arg", "move local var with rbp + positive offset to arg");

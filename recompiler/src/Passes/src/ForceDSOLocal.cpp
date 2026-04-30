#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct forceDSOLocalLegacy : public ModulePass {
  static char ID;
  forceDSOLocalLegacy() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    for (GlobalValue &GV : M.globals()) {
      GV.setDSOLocal(true);
    }

    for (Function &F : M) {
      if (F.isDeclaration()) {
        F.setDSOLocal(true);
      }
    }

    return true;
  }
};

class forceDSOLocal : public PassInfoMixin<forceDSOLocal> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    for (GlobalValue &GV : M.globals()) {
      GV.setDSOLocal(true);
    }

    for (Function &F : M) {
      if (F.isDeclaration()) {
        F.setDSOLocal(true);
      }
    }

    return PreservedAnalyses::all();
  }
};

} // end anonymous namespace

char forceDSOLocalLegacy::ID = 0;
static RegisterPass<forceDSOLocalLegacy> X("force-dso-local", "set all global variables to be dso_local");

llvm::PassPluginLibraryInfo getForceDSOLocalPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "forceDSOLocal", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "force-dso-local") {
                    MPM.addPass(forceDSOLocal());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getForceDSOLocalPluginInfo();
}

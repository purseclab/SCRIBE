
//===----------------------------------------------------------------------===//
// insertStackLayout
//===----------------------------------------------------------------------===//
namespace {
  struct insertStackLayout : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    insertStackLayout() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      errs() << "Function: ";
      errs().write_escaped(F.getName()) << '\n';
      auto StkLoc = MemoryBuffer::getFile(StkLocFileName);
      if (StkLoc.getError()) {
        errs() << "Error opening file: " << StkLocFileName << '\n';
        return false;
      }
      Expected<json::Value> StkLocJson = json::parse(StkLoc.get()->getBuffer());
      assert(StkLocJson && StkLocJson->kind() == json::Value::Object);
      json::Array *functions = StkLocJson->getAsObject()->getArray("functions");
      for (json::Value function : *functions) {
        json::Object *functionObj = function.getAsObject();
        if (F.getName() == function.getAsObject()->getString("name")) {
          json::Array *variables = functionObj->getArray("variables");
          for (json::Value variable : *variables) {
            json::Object *variableObj = variable.getAsObject();
            errs() << "Variable: " << variableObj->getString("name") << " " << variableObj->getInteger("StkLoc") << '\n';
          }
        }
      }

      // for (auto &BB : F) {
      //   for (auto &I : BB) {
      //     I.print(errs());
      //     errs() << '\n';
      //     if (I.hasMetadata(Metadata::MetadataKind::DIExpressionKind)) {
      //       errs() << "Found metadata\n";
      //     }
      //   }
      // }

      for (auto &BB : F) {
        for (auto &I : BB) {
          if (CallInst *callInst = dyn_cast<CallInst>(&I)) {
            if (Function *calledFunction = callInst->getCalledFunction()) {
             errs() << "Call: " << calledFunction->getName() << '\n'; 
            }
          }
        }
      }

      llvm::DISubprogram *subProgram = F.getSubprogram();
      subProgram->print(errs());
      errs() << '\n';

      return false;
    }
  };
}

char insertStackLayout::ID = 0;
static RegisterPass<insertStackLayout> X("insert-stack-layout", "Insert Stack Layout");

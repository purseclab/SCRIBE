#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/JSON.h"

using namespace clang;
using namespace llvm;

namespace {

class CorrectBoolTypeVisitor : public RecursiveASTVisitor<CorrectBoolTypeVisitor> {
public:
  explicit CorrectBoolTypeVisitor(Rewriter &R) : TheRewriter(R) {}

  bool VisitVarDecl(VarDecl *D) {
    if (D->getType().getAsString() == "bool") {
      errs() << "Found bool type: " << D->getNameAsString() << "\n";
      TheRewriter.ReplaceText(D->getBeginLoc(), 4, "_BOOL1");
    }
    return true;
  }
private:
  Rewriter &TheRewriter;
};

class CorrectBoolTypeConsumer : public ASTConsumer {
public:
  CorrectBoolTypeConsumer(Rewriter &R) : Visitor(R), TheRewriter(R) {}

  void HandleTranslationUnit(clang::ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    // Emit the rewritten code to stdout.
    TheRewriter.getEditBuffer(Context.getSourceManager().getMainFileID()).write(llvm::outs());
  }
private:
  CorrectBoolTypeVisitor Visitor;
  Rewriter &TheRewriter;  // Keep a reference to the Rewriter object
};


class CorrectBoolTypeAction : public PluginASTAction {
public:
  CorrectBoolTypeAction() : TheRewriter(std::make_unique<Rewriter>()) {}

protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef InFile) override {
    TheRewriter->setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<CorrectBoolTypeConsumer>(*TheRewriter);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }

private:
  std::unique_ptr<Rewriter> TheRewriter;
};

}

static FrontendPluginRegistry::Add<CorrectBoolTypeAction> X("correct-bool-type", "Correct Bool Type");

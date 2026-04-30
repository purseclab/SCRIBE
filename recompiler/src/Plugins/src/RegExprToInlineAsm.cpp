#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/JSON.h"

using namespace clang;
using namespace llvm;

// TODO:
// 1. read from config file figure out which local var is reg var
// 2. replace every use of such var with inline asm

namespace {

class RegExprToInlineAsmVisitor : public RecursiveASTVisitor<RegExprToInlineAsmVisitor> {
public:
  // explicit RegExprToInlineAsmVisitor(Rewriter &R) : TheRewriter(R) {}
  explicit RegExprToInlineAsmVisitor(Rewriter &R, json::Value &VarListJson) : TheRewriter(R), VarListJson(VarListJson) {}

  bool VisitFunctionDecl(FunctionDecl *f) {
    llvm::StringRef funcName(f->getNameInfo().getName().getAsString());
    json::Array *functions = VarListJson.getAsObject()->getArray("functions");
    for (json::Value function : *functions) {
        json::Object *functionObj = function.getAsObject();
        if (funcName == function.getAsObject()->getString("name")) {
            json::Array *variables = functionObj->getArray("variables");
            for (json::Value variable : *variables) {
              json::Object *variableObj = variable.getAsObject();
              if (!variableObj->getInteger("StkLoc")) {
                VarRegMap[variableObj->getString("name").getValue()] = variableObj->getString("StkLoc").getValue().str();
              }
            }
        }
    }

    return true;
  }

  bool VisitBinaryOperator(BinaryOperator *BO) {
    if (BO->isAssignmentOp()) {
      auto LHS = BO->getLHS();
      auto RHS = BO->getRHS();

      LHS = LHS->IgnoreImplicit()->IgnoreParenImpCasts();
      RHS = RHS->IgnoreImplicit()->IgnoreParenImpCasts();

      // if rhs is var, then we need to replace it with inline asm
      // something = v1 becomes asm("mov v1, %0" : "=r"(something))
      if (clang::isa<clang::DeclRefExpr>(RHS)) {
        auto Var = clang::dyn_cast<clang::DeclRefExpr>(RHS)->getDecl();
        if (VarRegMap.find(Var->getNameAsString()) != VarRegMap.end()) {
          auto LHS_str = TheRewriter.getRewrittenText(LHS->getSourceRange());
          auto inlineasm = "asm(\"mov %%" + VarRegMap[Var->getNameAsString()] + ", %0\" : \"=rm\"(" + LHS_str + "))";
          errs() << "Convert [" << TheRewriter.getRewrittenText(BO->getSourceRange()) << "] to [" << inlineasm << "]\n";
          TheRewriter.ReplaceText(BO->getSourceRange(), inlineasm);
        }
      }
      
    }
    return true;
  }
private:
  Rewriter &TheRewriter;
  json::Value &VarListJson;
  llvm::StringMap<std::string> VarRegMap;
};

class RegExprToInlineAsmConsumer : public ASTConsumer {
public:
  // RegExprToInlineAsmConsumer(Rewriter &R) : Visitor(R), TheRewriter(R) {}
  RegExprToInlineAsmConsumer(Rewriter &R, json::Value &VarListJson) : Visitor(R, VarListJson), TheRewriter(R), VarListJson(VarListJson) {}

  void HandleTranslationUnit(clang::ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    // Emit the rewritten code to stdout.
    TheRewriter.getEditBuffer(Context.getSourceManager().getMainFileID()).write(llvm::outs());
  }
private:
  RegExprToInlineAsmVisitor Visitor;
  Rewriter &TheRewriter;  // Keep a reference to the Rewriter object
  json::Value &VarListJson;
};


class RegExprToInlineAsmAction : public PluginASTAction {
public:
  RegExprToInlineAsmAction() : TheRewriter(std::make_unique<Rewriter>()) {}

protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef InFile) override {
    TheRewriter->setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<RegExprToInlineAsmConsumer>(*TheRewriter, *VarListJson);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    if (args.size() != 1) {
      return false;
    }

    auto VarList = MemoryBuffer::getFile(args[0]);
    if (VarList.getError()) {
      llvm::errs() << "Failed to open file: " << args[0] << "\n";
      return false;
    }

    Expected<json::Value> ExpectedVarListJson = json::parse(VarList.get()->getBuffer());
    assert(ExpectedVarListJson && ExpectedVarListJson->kind() == json::Value::Object);
    VarListJson = std::make_unique<json::Value>(*ExpectedVarListJson);

    return true;
  }

private:
  std::unique_ptr<Rewriter> TheRewriter;
  std::unique_ptr<json::Value> VarListJson;
};

}

static FrontendPluginRegistry::Add<RegExprToInlineAsmAction> X("reg-to-asm", "Register Expression To Inline Assembly");

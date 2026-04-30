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

static cl::opt<std::string> StkLocFileName("stkloc", cl::desc("Specify json filename for stack layout"), cl::value_desc("filename"));

bool isBpBased(llvm::Optional<llvm::StringRef> funcName) {
        //===----------------------------------------------------------------------===//
        // Parse JSON file
        //===----------------------------------------------------------------------===//
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
            if (funcName == function.getAsObject()->getString("name")) {
                return functionObj->getBoolean("is_bp_based").getValue();
            }
        }
        return false;
}

llvm::StringMap<int> parseStkLoc(llvm::Optional<llvm::StringRef> funcName) {
        //===----------------------------------------------------------------------===//
        // Parse JSON file
        //===----------------------------------------------------------------------===//
        llvm::StringMap<int> StkLocMap;
        auto StkLoc = MemoryBuffer::getFile(StkLocFileName);
        if (StkLoc.getError()) {
            errs() << "Error opening file: " << StkLocFileName << '\n';
            return StkLocMap;
        }
        Expected<json::Value> StkLocJson = json::parse(StkLoc.get()->getBuffer());

        assert(StkLocJson && StkLocJson->kind() == json::Value::Object);
        json::Array *functions = StkLocJson->getAsObject()->getArray("functions");
        for (json::Value function : *functions) {
            json::Object *functionObj = function.getAsObject();
            if (funcName == function.getAsObject()->getString("name")) {
                json::Array *variables = functionObj->getArray("variables");
                for (json::Value variable : *variables) {
                json::Object *variableObj = variable.getAsObject();
                if (variableObj->getInteger("StkLoc"))
                    StkLocMap[variableObj->getString("name").getValue()] = variableObj->getInteger("StkLoc").getValue();
                }
            }
        }
        return StkLocMap;
}


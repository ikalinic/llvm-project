#include "llvm/Transforms/Utils/RemoveDebugInst.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include <map>

using namespace llvm;

PreservedAnalyses RemoveDebugInstPass::run(Function &F,
                                      FunctionAnalysisManager &AM) {

  for (BasicBlock &bb : F) {
    for (Instruction &instr : make_early_inc_range(bb)) {
      if (instr.isDebugOrPseudoInst()) {
          if (isa<CallInst>(instr)) {
          StringRef functionName = cast<CallInst>(instr).getCalledFunction()->getName();
          if (functionName.starts_with("llvm.dbg")) {
            instr.eraseFromParent();
          }
          }
        }
    }
  }

  return PreservedAnalyses::all();
}

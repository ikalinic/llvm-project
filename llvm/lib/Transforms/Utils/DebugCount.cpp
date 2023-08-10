#include "llvm/Transforms/Utils/DebugCount.h"
#include "llvm/IR/Instructions.h"
#include <map>

using namespace llvm;

PreservedAnalyses DebugCountPass::run(Function &F,
                                      FunctionAnalysisManager &AM) {

  std::map<StringRef, int> debugInstructionCounts;

  for (BasicBlock& bb : F) {
    for (Instruction& instr : bb) {
      if (instr.isDebugOrPseudoInst()) {
          if (isa<CallInst>(instr)) {
            StringRef debugFunctionName = cast<CallInst>(instr).getCalledFunction()->getName();
            ++debugInstructionCounts[debugFunctionName];
          }
        }
    }
  }

  errs() << "Function: " << F.getName() << "\n";

  for (auto const & c : debugInstructionCounts) {
    errs() << "\t" << c.first << ": " << c.second << "\n";
  }

  return PreservedAnalyses::all();
}

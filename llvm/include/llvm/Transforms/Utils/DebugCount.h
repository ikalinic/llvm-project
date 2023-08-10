#ifndef LLVM_TRANSFORMS_UTILS_DEBUGCOUNT_H
#define LLVM_TRANSFORMS_UTILS_DEBUGCOUNT_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class DebugCountPass : public PassInfoMixin<DebugCountPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
  static bool isRequired() { return true; }
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_DEBUGCOUNT_H
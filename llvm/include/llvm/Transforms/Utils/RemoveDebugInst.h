#ifndef LLVM_TRANSFORMS_UTILS_REMOVEDEBUGINST_H
#define LLVM_TRANSFORMS_UTILS_REMOVEDEBUGINST_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class RemoveDebugInstPass : public PassInfoMixin<RemoveDebugInstPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
  static bool isRequired() { return true; }
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_REMOVEDEBUGINST_H

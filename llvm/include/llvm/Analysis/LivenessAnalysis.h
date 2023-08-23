#ifndef LLVM_ANALYSIS_LIVENESS_ANALYSIS_H
#define LLVM_ANALYSIS_LIVENESS_ANALYSIS_H

#include "llvm/IR/PassManager.h"
#include <map>
#include <set>
#include <vector>

namespace llvm {

class LivenessAnalysisPass : public PassInfoMixin<LivenessAnalysisPass> {
public:
  //   PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
  static bool isRequired() { return true; }

private:
  void livenessAnalysis(Function &F);
  void analyzeFunction(Function &F);
  void analyzeInstruction(Instruction &instr);
  std::vector<unsigned> counts;
  void increment(unsigned c);
  void printHistogram();
  std::set<llvm::StringRef> liveValues;
  std::map<Instruction *, std::set<llvm::StringRef>> live;
  std::map<Instruction *, std::set<llvm::StringRef>> def;
  std::map<Instruction *, std::set<llvm::StringRef>> use;
};

} // namespace llvm

#endif // LLVM_ANALYSIS_LIVENESS_ANALYSIS_H

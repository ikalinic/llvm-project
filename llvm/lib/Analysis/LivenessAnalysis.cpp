#include "llvm/Analysis/LivenessAnalysis.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include <map>
#include <set>

using namespace llvm;

PreservedAnalyses LivenessAnalysisPass::run(Module &M,
                                            ModuleAnalysisManager &MAM) {

  FunctionAnalysisManager &FAM =
      MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

  assert(not FAM.empty());

  for (Module::iterator mi = M.begin(); mi != M.end(); ++mi) {
    analyzeFunction(*mi);
    livenessAnalysis(*mi);
  }

  printHistogram();

  return PreservedAnalyses::all();
}

void LivenessAnalysisPass::analyzeFunction(Function &F) {
  for (BasicBlock &bb : F) {
    for (Instruction &instr : bb) {

      live[&instr] = std::set<StringRef>();

      analyzeInstruction(instr);
    }
  }
}

void LivenessAnalysisPass::analyzeInstruction(Instruction &instr) {

  if (isa<AllocaInst>(&instr) || isa<ReturnInst>(&instr)) {
    return;
  }

  User::op_iterator OI, OE;
  for (OI = instr.op_begin(), OE = instr.op_end(); OI != OE; ++OI) {
    Value *val = *OI;
    if (isa<Instruction>(val) || isa<Argument>(val)) { // val is used by insn

      if (isa<BranchInst>(&instr)) {
        if (cast<BranchInst>(&instr)->isConditional()) {

          use[&instr] = std::set<StringRef>({val->getName()});
        }
        return;

      } else if (isa<StoreInst>(instr) && *OI == instr.getOperand(1)) {

        def[&instr] = std::set<StringRef>({val->getName()});

        return;

      } else if (!isa<Constant>(val)) {
        if (use.find(&instr) == use.end()) {

          use[&instr] = std::set<StringRef>({val->getName()});

        } else {

          use[&instr].insert(val->getName());
        }
      }
    }
  }
  def[&instr] = std::set<StringRef>({instr.getName()});
}

void LivenessAnalysisPass::livenessAnalysis(Function &F) {

  bool liveSetChanged = true;

  while (liveSetChanged) {
    for (Function::iterator bb = --F.end(); bb != --F.begin(); bb--) {
      for (BasicBlock::reverse_iterator bri = bb->rbegin(); bri != bb->rend();
           bri++) {

        Instruction &instr = *bri;

        std::set<StringRef> defAtCurrentInstr =
            use.find(&instr) != use.end() ? use[&instr] : std::set<StringRef>();
        std::set<StringRef> useAtCurrentInstr =
            def.find(&instr) != def.end() ? def[&instr] : std::set<StringRef>();

        std::set<StringRef> liveTemp = std::set<StringRef>();

        std::set<StringRef> liveAfterCurrentInstruction;
        if (instr.getOpcode() != Instruction::Ret) {

          if (instr.isTerminator()) {

            for (unsigned int i = 0; i < instr.getNumSuccessors(); i++) {
              Instruction *searchedInstr = &(instr.getSuccessor(i)->front());
              for (StringRef name : live[searchedInstr]) {
                if (liveAfterCurrentInstruction.find(name) ==
                    liveAfterCurrentInstruction.end()) {
                  liveAfterCurrentInstruction.insert(name);
                }
              }
            }
          } else {
            Instruction *nextInstr = instr.getNextNonDebugInstruction();
            if (live.find(nextInstr) != live.end()) {
            liveAfterCurrentInstruction = live[nextInstr];
            }
          }
        } else {
          liveAfterCurrentInstruction = std::set<StringRef>();
        }

        for (StringRef name : liveAfterCurrentInstruction) {
          if (defAtCurrentInstr.find(name) == defAtCurrentInstr.end()) {
            liveTemp.insert(name);
          }
        }

        for (StringRef name : useAtCurrentInstr) {
          if (liveTemp.find(name) == liveTemp.end()) {
            liveTemp.insert(name);
          }
        }

        for (StringRef name : liveTemp) {
          if (live[&instr].find(name) == live[&instr].end()) {
            live[&instr].insert(name);
          } else {
            liveSetChanged = false;
          }
        }
      }
    }
  }

  for (const auto &item : live) {
    increment(item.second.size());
  }
}

void LivenessAnalysisPass::increment(unsigned c) {
  if (counts.size() <= c)
    counts.resize(c + 1, 0);
  counts[c]++;
}

void LivenessAnalysisPass::printHistogram() {
  for (unsigned i = 0, n = counts.size(); i < n; i++) {
    llvm::errs() << i << ": " << counts[i] << "\n";
  }
}

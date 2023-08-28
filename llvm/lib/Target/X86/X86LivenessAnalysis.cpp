#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

using namespace llvm;

#define LIVENESS_ANALYSIS_PASS_NAME "Machine IR liveness analysis"

namespace {

class X86LivenessAnalysis : public MachineFunctionPass {
public:
  static char ID;

  X86LivenessAnalysis() : MachineFunctionPass(ID) {
    initializeX86LivenessAnalysisPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return LIVENESS_ANALYSIS_PASS_NAME; }

private:
  void livenessAnalysis(MachineFunction &);
  void analyzeFunction(MachineFunction &);
  void analyzeInstruction(MachineInstr &, MachineRegisterInfo &);
  void printLive(MachineFunction &);
  std::vector<unsigned> counts;
  void increment(unsigned c);
  void printHistogram();
  std::set<llvm::StringRef> liveValues;
  std::map<MachineInstr *, std::set<StringRef>> live;
  std::map<MachineInstr *, std::set<StringRef>> def;
  std::map<MachineInstr *, std::set<StringRef>> use;
};

char X86LivenessAnalysis::ID = 0;

bool X86LivenessAnalysis::runOnMachineFunction(MachineFunction &MF) {

  analyzeFunction(MF);
  livenessAnalysis(MF);
  printHistogram();

  return false;
}

void X86LivenessAnalysis::analyzeFunction(MachineFunction &MF) {
  MachineRegisterInfo registerInfo = MachineRegisterInfo(&MF);

  for (MachineBasicBlock &bb : MF) {
    for (MachineInstr &instr : bb) {
      live[&instr] = std::set<StringRef>();
      use[&instr] = std::set<StringRef>();
      def[&instr] = std::set<StringRef>();
      analyzeInstruction(instr, registerInfo);
    }
  }
}

void X86LivenessAnalysis::analyzeInstruction(
    MachineInstr &instr, MachineRegisterInfo &registerInfo) {
  if (instr.isReturn()) {
    return;
  }

  MachineInstr::mop_iterator OI, OE;
  for (OI = instr.operands_begin(), OE = instr.operands_end(); OI != OE; ++OI) {
    MachineOperand &operand = *OI;

    if (operand.isReg() ) {
      const TargetRegisterInfo *tri = registerInfo.getTargetRegisterInfo();
      
      if(Register::isPhysicalRegister(operand.getReg())){
        StringRef regName = tri->getRegAsmName(operand.getReg().asMCReg());
        if (operand.isDef()) {  
        def[&instr].insert(regName);
        // errs() << "DEF of register: " << regName << "\n";
      } else if (operand.isUse()) {
        use[&instr].insert(regName);
        // errs() << "USE of register: " << regName << "\n";
      }
      }
    }
  }
}

void X86LivenessAnalysis::livenessAnalysis(MachineFunction &MF) {

  MachineRegisterInfo registerInfo = MachineRegisterInfo(&MF);
  const TargetRegisterInfo *tri = registerInfo.getTargetRegisterInfo();

  std::vector<MachineInstr *> worklist = std::vector<MachineInstr *>();

  for (MachineBasicBlock &bb : MF) {
    for (MachineInstr &instr : bb) {
      worklist.push_back(&instr);
    }
  }

  bool liveSetChanged = true;
  unsigned int lines = worklist.size() - 1;
  unsigned int passes = 1;


  

  while (liveSetChanged) {
    // LIVE[n] = use[n] U ( U (LIVE[n+1]) \ def[n] )

    errs() << "[Liveness analysis] Pass: " << passes << "\n";

    for (unsigned int i = lines; i >= 1; --i) {
      MachineInstr *nextInstr = worklist[i];
      MachineInstr *instr = worklist[i - 1];

      std::set<StringRef> defAtCurrentInstr =
          use.find(instr) != use.end() ? use[instr]
                                               : std::set<StringRef>();
      std::set<StringRef> useAtCurrentInstr =
          def.find(instr) != def.end() ? def[instr]
                                               : std::set<StringRef>();

      std::set<StringRef> liveTemp = std::set<StringRef>();

      std::set<StringRef> liveAtNextInstruction;

      if (!instr->isReturn()) {
          liveAtNextInstruction = live[nextInstr];
      } else {
        liveAtNextInstruction = std::set<StringRef>();
      }

      for (StringRef name : liveAtNextInstruction) {
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
          if (live[instr].find(name) == live[instr].end()) {
            live[instr].insert(name);
          } else {
            liveSetChanged = false;
          }
        }
        
    }
    passes++;
  }

  for(MachineInstr *instr : worklist) {
    errs() << "Instruction: " << *instr << "\n";
    errs() << "Live set at program point: {";
    for(StringRef name : live[instr]){
      errs() << name << " ";
    }
    errs() << "}\n";

    increment(live[instr].size());
  }
}

void X86LivenessAnalysis::printLive(MachineFunction &MF) {}

void X86LivenessAnalysis::increment(unsigned c) {
  if (counts.size() <= c)
    counts.resize(c + 1, 0);
  counts[c]++;
}

void X86LivenessAnalysis::printHistogram() {
  for (unsigned i = 0, n = counts.size(); i < n; i++) {
    llvm::errs() << i << ": " << counts[i] << "\n";
  }
}

} // end of anonymous namespace

INITIALIZE_PASS(X86LivenessAnalysis, "liveness-analysis-pass",
                LIVENESS_ANALYSIS_PASS_NAME, false, false)

namespace llvm {

FunctionPass *createX86LivenessAnalysisPass() {
  return new X86LivenessAnalysis();
}

} // namespace llvm

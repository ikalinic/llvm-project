#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

using namespace llvm;

#define REMOVE_DEBUG_MACHINE_INST_PASS_NAME "Remove debug machine instructions"

namespace {

class X86RemoveDebugMachineInst : public MachineFunctionPass {
public:

  static char ID;

  X86RemoveDebugMachineInst() : MachineFunctionPass(ID) {
    initializeX86RemoveDebugMachineInstPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction& MF) override;
  
  StringRef getPassName() const override { return REMOVE_DEBUG_MACHINE_INST_PASS_NAME; }
};

char X86RemoveDebugMachineInst::ID = 0;

bool X86RemoveDebugMachineInst::runOnMachineFunction(MachineFunction& MF) {
    for (MachineBasicBlock& mbb : MF) {
      for(MachineInstr& instr : make_early_inc_range(mbb)) {
        if (instr.isDebugOrPseudoInstr()) {
          instr.eraseFromParent();
        }
      }
    }

    return false;
}

} // end of anonymous namespace

INITIALIZE_PASS(X86RemoveDebugMachineInst, "remove-debug-machine-inst-pass",
  REMOVE_DEBUG_MACHINE_INST_PASS_NAME,
  true,
  false
)

namespace llvm {

FunctionPass *createX86RemoveDebugMachineInstPass() { return new X86RemoveDebugMachineInst(); }

}

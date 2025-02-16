#include "Pass/Analysis.h"
#include "Pass/Transform.h"

void execute_O0_passes(std::shared_ptr<Mir::Module> &module) {
    const auto cfg = Pass::Pass::create<Pass::ControlFlowGraph>();
    module = module | cfg;
    const auto mem2reg = Pass::Pass::create<Pass::Mem2Reg>(cfg);
    module = module | mem2reg;
    const auto constant_folding = Pass::Pass::create<Pass::ConstantFolding>();
    module = module | constant_folding;
}

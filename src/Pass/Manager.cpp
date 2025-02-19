#include "Pass/Analysis.h"
#include "Pass/Transform.h"

void execute_O0_passes(std::shared_ptr<Mir::Module> &module) {
    auto mem2reg = Pass::Pass::create<Pass::Mem2Reg>();
    module = module | mem2reg;
    auto constant_folding = Pass::Pass::create<Pass::ConstantFolding>();
    module = module | constant_folding;
    auto simplify_cfg = Pass::Pass::create<Pass::SimplifyCFG>();
    module = module | simplify_cfg;
    constant_folding = Pass::Pass::create<Pass::ConstantFolding>();
    module = module | constant_folding;
    auto dead_func = Pass::Pass::create<Pass::DeadFuncEliminate>();
    module = module | dead_func;
}

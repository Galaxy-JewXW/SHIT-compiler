#include "Pass/Analysis.h"
#include "Pass/Transform.h"

void execute_O0_passes(std::shared_ptr<Mir::Module> &module) {
    const auto mem2reg = Pass::Pass::create<Pass::Mem2Reg>();
    module = module | mem2reg;
    auto constant_folding = Pass::Pass::create<Pass::ConstantFolding>();
    module = module | constant_folding;
    const auto simplify_cfg = Pass::Pass::create<Pass::SimplifyCFG>();
    module = module | simplify_cfg;
    constant_folding = Pass::Pass::create<Pass::ConstantFolding>();
    module = module | constant_folding;
    const auto dead_func = Pass::Pass::create<Pass::DeadFuncEliminate>();
    module = module | dead_func;
    const auto dead_inst = Pass::Pass::create<Pass::DeadInstEliminate>();
    module = module | dead_inst;
    const auto algebraic = Pass::Pass::create<Pass::AlgebraicSimplify>();
    module = module | algebraic;
//    const auto loop_simply_form = Pass::Pass::create<Pass::LoopSimplyForm>();
//    module = module | loop_simply_form;
}

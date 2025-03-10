#include "Pass/Analysis.h"
#include "Pass/Transform.h"
#include "Pass/Util.h"

[[maybe_unused]] void execute_O0_passes(std::shared_ptr<Mir::Module> &module) {}

void execute_O1_passes(std::shared_ptr<Mir::Module> &module) {
    apply<
        Pass::Mem2Reg,
        Pass::ConstantFolding,
        Pass::SimplifyCFG,
        Pass::ConstantFolding,
        Pass::EmitModule<>,
        Pass::DeadFuncEliminate,
        Pass::DeadInstEliminate,
        Pass::AlgebraicSimplify,
        Pass::EmitModule<>,
        Pass::SimplifyCFG
    >(module);
}

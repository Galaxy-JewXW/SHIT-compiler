#include "Pass/Analysis.h"
#include "Pass/Transform.h"
#include "Pass/Util.h"

[[maybe_unused]] void execute_O0_passes(std::shared_ptr<Mir::Module> &module) {}

void execute_O1_passes(std::shared_ptr<Mir::Module> &module) {
    apply<
        Pass::Mem2Reg,
        Pass::GlobalValueNumbering,
        Pass::SimplifyCFG,
        Pass::DeadFuncEliminate,
        Pass::EmitModule<true>,
        Pass::GlobalValueNumbering,
        Pass::LoopSimplyForm,
        Pass::LCSSA,
        Pass::DeadInstEliminate,
        Pass::DeadCodeEliminate,
        Pass::DeadFuncArgEliminate,
        Pass::ConstexprFuncEval,
        Pass::DeadFuncEliminate,
        Pass::DeadCodeEliminate,
        Pass::DeadReturnEliminate,
        Pass::GlobalValueNumbering,
        Pass::SimplifyCFG,
        Pass::GlobalVariableLocalize
    >(module);
}

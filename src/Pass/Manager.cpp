#include "Pass/Transforms/Array.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Transforms/DCE.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Transforms/Loop.h"
#include "Pass/Util.h"

[[maybe_unused]]
void execute_O0_passes(std::shared_ptr<Mir::Module> &module) {
    apply<Pass::Mem2Reg, Pass::GlobalValueNumbering, Pass::GepFolding>(module);
    apply<Pass::RemovePhi, Pass::SimplifyControlFlow>(module);
}

void execute_O1_passes(std::shared_ptr<Mir::Module> &module) {
    apply<Pass::Mem2Reg, Pass::AlgebraicSimplify,
          Pass::BranchMerging, Pass::GepFolding, Pass::GlobalVariableLocalize,
          Pass::GlobalArrayLocalize, Pass::LoadEliminate, Pass::StoreEliminate, Pass::SROA, Pass::ConstexprFuncEval,
          Pass::GlobalValueNumbering, Pass::TailCallOptimize>(module);
    apply<Pass::DeadFuncEliminate, Pass::DeadFuncArgEliminate, Pass::DeadReturnEliminate>(module);
    apply<Pass::ConstrainReduce, Pass::SimplifyControlFlow>(module);
    apply<Pass::Reassociate, Pass::SimplifyControlFlow>(module);
    apply<Pass::RemovePhi, Pass::SimplifyControlFlow, Pass::BlockPositioning>(module);
}

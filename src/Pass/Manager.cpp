#include "Pass/Transforms/Array.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Transforms/DCE.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Transforms/Loop.h"
#include "Pass/Util.h"

[[maybe_unused]]
void execute_O0_passes(std::shared_ptr<Mir::Module> &module) {
    try {
        apply<Pass::Mem2Reg>(module);
    } catch (const std::invalid_argument &) {
        apply<Pass::AlgebraicSimplify>(module);
        apply<Pass::SimplifyControlFlow>(module);
        module->update_id();
        return;
    }
    apply<Pass::LocalValueNumbering, Pass::GepFolding>(module);
    apply<Pass::LocalValueNumbering, Pass::SimplifyControlFlow>(module);
    apply<Pass::GlobalVariableLocalize>(module);
    apply<Pass::GlobalArrayLocalize>(module);
    apply<Pass::LoadEliminate>(module);
    apply<Pass::StoreEliminate>(module);
    apply<Pass::AlgebraicSimplify>(module);
    apply<Pass::LocalValueNumbering, Pass::SimplifyControlFlow>(module);
    apply<Pass::DeadCodeEliminate>(module);
    apply<Pass::ConstexprFuncEval<>>(module);
    apply<Pass::DeadFuncEliminate>(module);
    apply<Pass::RemovePhi>(module);

    module->update_id();
}

void execute_O1_passes(std::shared_ptr<Mir::Module> &module) {
    try {
        apply<Pass::Mem2Reg>(module);
    } catch (const std::invalid_argument &) {
        apply<Pass::AlgebraicSimplify>(module);
        apply<Pass::SimplifyControlFlow>(module);
        module->update_id();
        return;
    }
    apply<Pass::LocalValueNumbering, Pass::GepFolding>(module);
    apply<Pass::DeadCodeEliminate>(module);
    apply<Pass::LocalValueNumbering, Pass::SimplifyControlFlow>(module);
    apply<Pass::Inlining, Pass::DeadFuncEliminate>(module);
    // apply<Pass::TailCallOptimize>(module);
    apply<Pass::GlobalVariableLocalize>(module);
    apply<Pass::GlobalArrayLocalize>(module);
    apply<Pass::LoadEliminate>(module);
    apply<Pass::StoreEliminate>(module);
    apply<Pass::AlgebraicSimplify>(module);
    apply<Pass::LocalValueNumbering, Pass::SimplifyControlFlow>(module);
    apply<Pass::DeadCodeEliminate>(module);
    apply<Pass::ConstexprFuncEval<>>(module);
    // apply<Pass::DeadFuncEliminate, Pass::DeadFuncArgEliminate, Pass::DeadReturnEliminate>(module);
    apply<Pass::ConstrainReduce>(module);
    apply<Pass::RemovePhi, Pass::BlockPositioning<1>>(module);

    module->update_id();
}

#include "Pass/Transforms/Array.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Transforms/DCE.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Transforms/Loop.h"
#include "Pass/Util.h"

[[maybe_unused]]
void execute_O0_passes(std::shared_ptr<Mir::Module> &module) {
    apply<Pass::Mem2Reg, Pass::GlobalValueNumbering>(module);
}

void execute_O1_passes(std::shared_ptr<Mir::Module> &module) {
    apply<Pass::Mem2Reg, Pass::TreeHeightBalance, Pass::AlgebraicSimplify,
          // Pass::LoopSimplyForm,
          // Pass::LCSSA,
          Pass::DeadCodeEliminate, Pass::BranchMerging, Pass::GepFolding, Pass::GlobalVariableLocalize,
          Pass::GlobalArrayLocalize, Pass::LoadEliminate, Pass::StoreEliminate, Pass::SROA, Pass::ConstexprFuncEval,
          Pass::DeadCodeEliminate, Pass::GlobalValueNumbering, Pass::TailCallOptimize, Pass::BlockPositioning,
          Pass::SimplifyControlFlow, Pass::ConstrainReduce>(module);
}

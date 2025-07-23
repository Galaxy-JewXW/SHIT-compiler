#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace Pass {
void SingleReturnTransform::run_on_func(const std::shared_ptr<Function> &func) {
    const auto &blocks{func->get_blocks()};
    std::unordered_map<std::shared_ptr<Block>, std::shared_ptr<Ret>> rets;
    const auto return_cnt = std::count_if(blocks.begin(), blocks.end(), [&rets](const auto &block) {
        if (const auto ret{block->get_instructions().back()}; ret->get_op() == Operator::RET) {
            rets[block] = ret->template as<Ret>();
            return true;
        }
        return false;
    });
    if (return_cnt < 2) {
        return;
    }
    const auto ret_block{Block::create("ret_block", func)};
    for (const auto &[block, ret]: rets) {
        block->get_instructions().pop_back();
        if (!block->get_instructions().empty()) {
            Jump::create(ret_block, block);
        } else {
            block->replace_by_new_value(ret_block);
            block->set_deleted();
        }
    }
    set_analysis_result_dirty<ControlFlowGraph>(func);
    if (func->get_return_type()->is_void()) {
        Ret::create(ret_block);
        return;
    }
    const auto phi{Phi::create("ret.phi", func->get_return_type(), ret_block, {})};
    for (const auto &[block, ret]: rets) {
        if (ret->get_operands().empty()) [[unlikely]] {
            log_error("Ret should have a return value");
        }
        if (const auto ret_phi{ret->get_value()->is<Phi>()}) {
            for (const auto &[ret_block, value]: ret_phi->get_optional_values()) {
                phi->set_optional_value(ret_block, value);
            }
        } else {
            phi->set_optional_value(block, ret->get_value());
        }
    }
    Ret::create(phi, ret_block);
    SimplifyControlFlow::remove_deleted_blocks(func);
}

void SingleReturnTransform::transform(const std::shared_ptr<Module> module) {
    for (const auto &func: *module) {
        run_on_func(func);
    }
}

void SingleReturnTransform::transform(const std::shared_ptr<Function> &func) { run_on_func(func); }
} // namespace Pass

#include <functional>
#include <unordered_set>

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
void remove_unreachable_blocks(const std::shared_ptr<Function> &func) {
    std::unordered_set<std::shared_ptr<Block>> visited_blocks;

    const std::function<void(const std::shared_ptr<Block> &)> dfs = [&](const std::shared_ptr<Block> &block) -> void {
        if (visited_blocks.count(block)) {
            return;
        }
        visited_blocks.insert(block);
        const auto &instructions = block->get_instructions();
        if (instructions.empty()) {
            log_error("Empty Block");
        }
        const auto last_instruction = instructions.back();
        if (const auto op = last_instruction->get_op(); op == Operator::JUMP) {
            dfs(last_instruction->as<Jump>()->get_target_block());
        } else if (op == Operator::BRANCH) {
            const auto branch = last_instruction->as<Branch>();
            dfs(branch->get_true_block());
            dfs(branch->get_false_block());
        } else if (op != Operator::RET) {
            log_error("Last instruction is not a terminator: %s", last_instruction->to_string().c_str());
        }
    };

    dfs(func->get_blocks().front());

    auto &blocks = func->get_blocks();
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [&](const std::shared_ptr<Block> &block) {
        if (visited_blocks.find(block) != visited_blocks.end()) {
            return false;
        }
        std::for_each(block->get_instructions().begin(), block->get_instructions().end(),
                      [&](const std::shared_ptr<Instruction> &instruction) {
                          instruction->clear_operands();
                      });
        block->clear_operands();
        block->set_deleted();
        Pass::get_analysis_result<Pass::ControlFlowGraph>(Module::instance())->set_dirty(func);
        return true;
    }), blocks.end());
}
}

namespace Pass {
void SimplifyControlGraph::run_on_func(const std::shared_ptr<Function> &func) const {
    const auto graph = cfg_info->graph(func);
    bool graph_modified{false}, changed{false};

    const auto fold_redundant_branch = [&]() -> bool {
        for (const auto &block : func->get_blocks()) {
            auto &last_instruction = block->get_instructions().back();
            if (last_instruction->get_op() != Operator::BRANCH) {
                continue;
            }
            const auto branch = last_instruction->as<Branch>();
            if (const auto cond = branch->get_cond(); cond->is_constant()) {
                const auto cond_value = cond->is<ConstBool>();
                if (cond_value == nullptr) {
                    log_error("Cond is not a ConstBool object");
                }
            }
        }
    };

    do {
        changed = false;
    } while (changed);

    if (graph_modified) {
        cfg_info->set_dirty(func);
    }
}

void SimplifyControlGraph::transform(const std::shared_ptr<Module> module) {
    // 预处理：清除不可达基本块
    for (const auto &func: *module) {
        remove_unreachable_blocks(func);
    }

    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
    cfg_info = nullptr;
}
}

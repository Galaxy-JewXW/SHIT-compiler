#include <unordered_set>
#include "Pass/Transform.h"

using namespace Mir;

static std::unordered_set<std::shared_ptr<Block>> visited{};

namespace Pass {
static void dfs(const std::shared_ptr<Block> &current_block) {
    if (visited.find(current_block) != visited.end()) return;
    visited.insert(current_block);
    if (current_block->get_instructions().empty()) { log_error("Empty Block"); }
    const auto last_instruction = current_block->get_instructions().back();
    if (const auto op = last_instruction->get_op(); op == Operator::JUMP) {
        const auto jump = std::static_pointer_cast<Jump>(last_instruction);
        dfs(jump->get_target_block());
    } else if (op == Operator::BRANCH) {
        const auto branch = std::static_pointer_cast<Branch>(last_instruction);
        if (const auto cond = branch->get_cond(); cond->is_constant()) {
            const auto cond_value = std::dynamic_pointer_cast<ConstBool>(cond);
            if (cond_value == nullptr) { log_error("Cond is not a ConstBool object"); }
            if (std::any_cast<int>(cond_value->get_constant_value())) {
                const auto jump_true = Jump::create(branch->get_true_block(), nullptr);
                jump_true->set_block(current_block, false);
                branch->replace_by_new_value(jump_true);
                current_block->get_instructions().back() = jump_true;
                dfs(branch->get_true_block());
            } else {
                const auto jump_false = Jump::create(branch->get_false_block(), nullptr);
                jump_false->set_block(current_block, false);
                branch->replace_by_new_value(jump_false);
                current_block->get_instructions().back() = jump_false;
                dfs(branch->get_false_block());
            }
            branch->clear_operands();
        } else {
            dfs(branch->get_true_block());
            dfs(branch->get_false_block());
        }
    } else if (op != Operator::RET) {
        log_error("Last instruction is not a terminator: %s", last_instruction->to_string().c_str());
    }
}

static void remove_deleted_blocks(const std::shared_ptr<Phi> &phi) {
    for (auto it = phi->get_optional_values().begin(); it != phi->get_optional_values().end();) {
        if (auto &[block, value] = *it; block->is_deleted()) {
            value->delete_user(phi);
            it = phi->get_optional_values().erase(it);
        } else {
            ++it;
        }
    }
}

static bool all_operands_equal(const std::shared_ptr<Phi> &phi) {
    const auto &values = phi->get_optional_values();
    if (values.empty()) { log_fatal("Phi has no optional values");}
    if (values.size() == 1) return true;
    const auto &first_val = values.begin()->second;
    return std::all_of(values.begin(), values.end(),
                       [&](const auto &entry) { return entry.second == first_val; });
}


static void run_on_function(const std::shared_ptr<Function> &func) {
    // 遍历控制流删除无法到达的block
    visited.clear();
    const auto entry_block = func->get_blocks().front();
    dfs(entry_block);
    auto &blocks = func->get_blocks();
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [](const std::shared_ptr<Block> &block) {
        if (visited.find(block) != visited.end()) {
            return false;
        }
        std::for_each(block->get_instructions().begin(), block->get_instructions().end(),
                      [&](const std::shared_ptr<Instruction> &instruction) {
                          instruction->clear_operands();
                      });
        block->clear_operands();
        block->set_deleted();
        return true;
    }), blocks.end());
    // 对于无法到达的block，清理有关的phi节点
    for (const auto &block: blocks) {
        for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
            if ((*it)->get_op() != Operator::PHI) break;
            auto phi = std::static_pointer_cast<Phi>(*it);
            remove_deleted_blocks(phi);
            if (all_operands_equal(phi) || phi->users().size() == 0) {
                auto first_val = phi->get_optional_values().begin()->second;
                phi->replace_by_new_value(first_val);
                phi->clear_operands();
                it = block->get_instructions().erase(it);
            } else {
                ++it;
            }
        }
    }
}

void SimplifyCFG::transform(const std::shared_ptr<Module> module) {
    for (const auto &func: *module) {
        run_on_function(func);
    }
}
}

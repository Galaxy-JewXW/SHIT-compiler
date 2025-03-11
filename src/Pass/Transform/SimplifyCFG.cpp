#include <unordered_set>

#include "Pass/Analysis.h"
#include "Pass/Transform.h"

using namespace Mir;

static std::unordered_set<std::shared_ptr<Block>> visited{};

static std::shared_ptr<Pass::ControlFlowGraph> cfg_info = nullptr;

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

// 删除phi中无法到达的键值对（block已被删除或block并非phi的直接前继节点）
static void remove_unreachable_blocks_for_phi(const std::shared_ptr<Phi> &phi, const std::shared_ptr<Function> &func) {
    const auto &current_block = phi->get_block();
    auto block_is_unreachable = [&](const std::shared_ptr<Block> &block) -> bool {
        if (block->is_deleted()) {
            return true;
        }
        if (const auto &succ = cfg_info->predecessors(func).at(current_block);
            succ.find(block) == succ.end()) {
            return true;
        }
        return false;
    };
    for (auto it = phi->get_optional_values().begin(); it != phi->get_optional_values().end();) {
        if (auto &[block, value] = *it; block_is_unreachable(block)) {
            // remove_operand包括了delete_user的步骤
            phi->remove_operand(value);
            // block不是phi的操作数，这里只需要delete_user即可
            block->delete_user(phi);
            it = phi->get_optional_values().erase(it);
        } else {
            ++it;
        }
    }
}

static bool all_operands_equal(const std::shared_ptr<Phi> &phi) {
    const auto &values = phi->get_optional_values();
    if (values.empty()) {
        log_fatal("Phi has no optional values");
    }
    if (values.size() == 1)
        return true;
    const auto &first_val = values.begin()->second;
    return std::all_of(values.begin(), values.end(),
                       [&](const auto &entry) { return entry.second->get_name() == first_val->get_name(); });
}

static void perform_merge(const std::shared_ptr<Block> &block, const std::shared_ptr<Block> &child) {
    // child的唯一前驱是block，child向block合并
    const auto last_instruction = block->get_instructions().back();
    last_instruction->clear_operands();
    block->get_instructions().pop_back();
    // 将child的所有指令移动到block中
    for (auto it = child->get_instructions().begin(); it != child->get_instructions().end();) {
        auto instruction = *it;
        if (const auto op = instruction->get_op(); op == Operator::PHI) {
            const auto phi = std::static_pointer_cast<Phi>(instruction);
            if (phi->get_optional_values().find(block) != phi->get_optional_values().end()) {
                auto optional_value = phi->get_optional_values().at(block);
                phi->replace_by_new_value(optional_value);
            }
            phi->clear_operands();
        } else {
            instruction->set_block(block);
        }
        it = child->get_instructions().erase(it);
    }
    child->replace_by_new_value(block);
    child->set_deleted();
}

// 如果一个块有唯一的前驱，是前驱的唯一后继，则将当前块合并到前驱块
static bool try_merge_blocks(const std::shared_ptr<Function> &func) {
    bool changed = false;
    auto &blocks = func->get_blocks();
    for (const auto &block: blocks) {
        if (block->is_deleted())
            continue;
        if (cfg_info->successors(func).at(block).size() != 1)
            continue;
        const auto &child = *cfg_info->successors(func).at(block).begin();
        if (child->is_deleted())
            continue;
        if (cfg_info->predecessors(func).at(child).size() != 1)
            continue;
        if (const auto parent = *cfg_info->predecessors(func).at(child).begin(); parent != block) {
            log_error("Parent block is not the current block");
        }
        perform_merge(block, child);
        changed = true;
    }
    if (changed) [[unlikely]] {
        blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [](const std::shared_ptr<Block> &block) {
            if (!block->is_deleted()) [[likely]] {
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
    }
    return changed;
}

// 消除只包含单个非条件跳转的基本块
static bool try_simplify_single_jump(const std::shared_ptr<Function> &func) {
    bool changed = false;
    auto is_single_jump_block = [&](const std::shared_ptr<Block> &block) -> std::shared_ptr<Block> {
        if (block->get_instructions().size() != 1)
            return nullptr;
        if (cfg_info->predecessors(func).at(block).empty()) {
            return nullptr;
        }
        if (const auto op = block->get_instructions().front()->get_op(); op == Operator::JUMP) {
            return std::static_pointer_cast<Jump>(block->get_instructions().front())->get_target_block();
        }
        return nullptr;
    };
    auto &blocks = func->get_blocks();
    for (const auto &block: blocks) {
        if (block->is_deleted())
            continue;
        const auto target_block = is_single_jump_block(block);
        if (target_block == nullptr)
            continue;
        if (cfg_info->successors(func).at(block).size() != 1)
            log_error("Block has more than one successor");
        block->cleanup_users();
        const auto copied_users = block->weak_users();
        for (const auto &user: copied_users) {
            if (const auto sp = user.lock()) {
                if (const auto phi = std::dynamic_pointer_cast<Phi>(sp)) {
                    // 对于使用block的phi指令，应将其替换为block的前驱
                    auto &optional_values = phi->get_optional_values();
                    auto it = optional_values.find(block);
                    if (it == optional_values.end()) { log_error("Phi operand not found"); }
                    const auto value = it->second;
                    block->delete_user(phi);
                    optional_values.erase(it);
                    for (const auto &prev: cfg_info->predecessors(func).at(block)) {
                        phi->set_optional_value(prev, value);
                        prev->add_user(phi);
                    }
                } else {
                    sp->modify_operand(block, target_block);
                }
            }
        }
        block->weak_users().clear();
        block->set_deleted();
        changed = true;
    }
    if (changed) [[unlikely]] {
        blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [](const std::shared_ptr<Block> &block) {
            if (!block->is_deleted()) [[likely]] {
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
    }
    return changed;
}

static void remove_unreachable_blocks(const std::shared_ptr<Function> &func) {
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
}

static void remove_phi(const std::shared_ptr<Function> &func) {
    // 对于无法到达的block，清理有关的phi节点
    // 如果phi节点的每个可选值都是相同的，则替换为第一个可选值，并删除phi节点
    // 如果没有指令使用phi节点，则删除phi节点
    auto &blocks = func->get_blocks();
    for (const auto &block: blocks) {
        for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
            if ((*it)->get_op() != Operator::PHI) break;
            auto phi = std::static_pointer_cast<Phi>(*it);
            remove_unreachable_blocks_for_phi(phi, func);
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
        remove_unreachable_blocks(func);
    }
    cfg_info = create<ControlFlowGraph>();
    cfg_info->run_on(module);
    for (const auto &func: *module) {
        remove_phi(func);
        while (try_merge_blocks(func)) {
            cfg_info->run_on(module);
        }
        while (try_simplify_single_jump(func)) {
            cfg_info->run_on(module);
        }
        remove_phi(func);
    }
    cfg_info = nullptr;
}
}

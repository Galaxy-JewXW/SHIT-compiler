#include <unordered_set>

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
void try_constant_fold(const std::shared_ptr<Function> &func) {
    for (const auto &block: func->get_blocks()) {
        for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
            if (Pass::GlobalValueNumbering::fold_instruction(*it)) {
                (*it)->clear_operands();
                it = block->get_instructions().erase(it);
            } else {
                ++it;
            }
        }
    }
}

void perform_merge(const std::shared_ptr<Block> &block, const std::shared_ptr<Block> &child) {
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

void cleanup_phi(const std::shared_ptr<Function> &func, const std::shared_ptr<Pass::ControlFlowGraph> &cfg_info) {
    const auto all_options_equal = [&](const std::shared_ptr<Phi> &phi) -> bool {
        const auto &optional_values = phi->get_optional_values();
        if (optional_values.empty()) {
            log_error("Phi has not optional values");
        }
        if (optional_values.size() == 1) {
            return true;
        }
        const auto &first_val = optional_values.begin()->second;
        return std::all_of(optional_values.begin(), optional_values.end(),
                           [&](const auto &pair) { return pair.second->get_name() == first_val->get_name(); });
    };

    const auto remove_unreachable_phi_pairs = [&](const std::shared_ptr<Phi> &phi) -> void {
        const auto &current_block = phi->get_block();
        const auto block_is_unreachable = [&](const std::shared_ptr<Block> &block) -> bool {
            if (block->is_deleted()) {
                return true;
            }
            if (const auto &succ = cfg_info->graph(func).predecessors.at(current_block);
                succ.find(block) == succ.end()) {
                return true;
            }
            return false;
        };
        std::vector<std::shared_ptr<Block>> to_be_deleted;
        for (auto &[block, value]: phi->get_optional_values()) {
            if (block_is_unreachable(block)) {
                to_be_deleted.push_back(block);
            }
        }
        // 同时维护 operands 列表和 phi 指令自身持有的 optional_values
        std::for_each(to_be_deleted.begin(), to_be_deleted.end(),
                      [&](const auto &block) { phi->remove_optional_value(block); });
    };

    const auto &blocks = func->get_blocks();
    for (const auto &block: blocks) {
        for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
            if ((*it)->get_op() != Operator::PHI)
                break;
            auto phi = std::static_pointer_cast<Phi>(*it);
            remove_unreachable_phi_pairs(phi);
            if (all_options_equal(phi) || phi->users().size() == 0) {
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
} // namespace

namespace Pass {
void SimplifyControlFlow::remove_deleted_blocks(const std::shared_ptr<Function> &func) {
    auto &blocks = func->get_blocks();
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                [](const std::shared_ptr<Block> &block) {
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
                                }),
                 blocks.end());

    set_analysis_result_dirty<ControlFlowGraph>(func);
    set_analysis_result_dirty<DominanceGraph>(func);
}

void SimplifyControlFlow::remove_unreachable_blocks(const std::shared_ptr<Function> &func) {
    std::unordered_set<std::shared_ptr<Block>> visited_blocks;

    auto dfs = [&](auto &&self, const std::shared_ptr<Block> &block) -> void {
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
            self(self, last_instruction->as<Jump>()->get_target_block());
        } else if (op == Operator::BRANCH) {
            const auto branch = last_instruction->as<Branch>();
            self(self, branch->get_true_block());
            self(self, branch->get_false_block());
        } else if (op == Operator::SWITCH) {
            const auto switch_ = last_instruction->as<Switch>();
            self(self, switch_->get_default_block());
            for (const auto &[value, block]: switch_->cases()) {
                self(self, block);
            }
        } else if (op != Operator::RET) {
            log_error("Last instruction is not a terminator: %s", last_instruction->to_string().c_str());
        }
    };

    dfs(dfs, func->get_blocks().front());

    auto &blocks = func->get_blocks();
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                [&](const std::shared_ptr<Block> &block) {
                                    if (visited_blocks.find(block) != visited_blocks.end()) {
                                        return false;
                                    }
                                    std::for_each(block->get_instructions().begin(), block->get_instructions().end(),
                                                  [&](const std::shared_ptr<Instruction> &instruction) {
                                                      instruction->clear_operands();
                                                  });
                                    block->clear_operands();
                                    block->set_deleted();
                                    set_analysis_result_dirty<ControlFlowGraph>(func);
                                    return true;
                                }),
                 blocks.end());

    set_analysis_result_dirty<ControlFlowGraph>(func);
    set_analysis_result_dirty<DominanceGraph>(func);
}

void SimplifyControlFlow::run_on_func(const std::shared_ptr<Function> &func) const {
    auto predecessors = cfg_info->graph(func).predecessors;
    auto successors = cfg_info->graph(func).successors;
    bool graph_modified{false}, changed{false};

    // 合并冗余分支：分支指令的两个目标为同一个块，或者分支指令的条件变量为常数
    [[maybe_unused]] const auto fold_redundant_branch = [&]() -> void {
        for (const auto &block: func->get_blocks()) {
            if (block->is_deleted()) {
                continue;
            }
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
                const auto target_block = cond_value->get_constant_value().get<int>()
                                              ? branch->get_true_block()
                                              : branch->get_false_block();
                const auto jump = Jump::create(target_block, nullptr);
                jump->set_block(block, false);
                last_instruction->replace_by_new_value(jump);
                last_instruction = jump;
                // 手动更新
                if (target_block == branch->get_true_block()) {
                    successors.at(block).erase(branch->get_false_block());
                    predecessors.at(branch->get_false_block()).erase(block);
                } else {
                    successors.at(block).erase(branch->get_true_block());
                    predecessors.at(branch->get_true_block()).erase(block);
                }
                continue;
            }
            if (branch->get_true_block() == branch->get_false_block()) {
                const auto jump = Jump::create(branch->get_true_block(), nullptr);
                jump->set_block(block, false);
                last_instruction->replace_by_new_value(jump);
                last_instruction = jump;
            }
        }
    };

    // 合并基本块：如果一个块有唯一的前驱，是前驱的唯一后继，则将当前块合并到前驱块
    [[maybe_unused]] const auto combine_blocks = [&]() -> void {
        const auto &blocks = func->get_blocks();
        auto modified{false};
        for (const auto &block: blocks) {
            if (block->is_deleted()) {
                continue;
            }
            if (successors.at(block).size() != 1) {
                continue;
            }
            const auto child = *successors.at(block).begin();
            if (child->is_deleted()) {
                continue;
            }
            if (predecessors.at(child).size() != 1) {
                continue;
            }
            if (const auto parent = *predecessors.at(child).begin(); parent != block) {
                log_error("Parent block is not the current block");
            }
            perform_merge(block, child);
            modified = true;
            graph_modified = true;
            // 手动维护cfg
            successors.at(block).erase(child);
            successors.at(block).insert(successors.at(child).begin(), successors.at(child).end());
            std::for_each(successors.at(child).begin(), successors.at(child).end(), [&](const auto &b) {
                predecessors.at(b).erase(child);
                predecessors.at(b).insert(block);
            });
            predecessors.erase(child);
            successors.erase(child);
        }
        if (modified) {
            changed = true;
            remove_deleted_blocks(func);
        }
    };

    const auto is_single_jump_block = [&](const std::shared_ptr<Block> &block) -> std::shared_ptr<Block> {
        if (block->get_instructions().size() != 1) {
            return nullptr;
        }
        if (predecessors.at(block).empty()) {
            return nullptr;
        }
        if (block->get_instructions().front()->get_op() == Operator::JUMP) {
            const auto jump = block->get_instructions().front()->as<Jump>();
            return jump->get_target_block();
        }
        return nullptr;
    };

    const auto is_single_branch_block = [&](const std::shared_ptr<Block> &block) -> std::shared_ptr<Branch> {
        if (block->get_instructions().size() != 1) {
            return nullptr;
        }
        if (predecessors.at(block).empty()) {
            return nullptr;
        }
        if (block->get_instructions().front()->get_op() == Operator::BRANCH) {
            return block->get_instructions().front()->as<Branch>();
        }
        return nullptr;
    };

    // 检查给定块的前驱是否均只有给定块作为后继
    const auto get_candidate_predecessors = [&](const std::shared_ptr<Block> &block) {
        std::unordered_set<std::shared_ptr<Block>> candidates;
        for (const auto &pre: predecessors.at(block)) {
            if (successors.at(pre).size() != 1) {
                continue;
            }
            if (*successors.at(pre).begin() != block) {
                continue;
            }
            candidates.insert(pre);
        }
        return candidates;
    };

    // 移除"空"块：消除只包含单个无条件跳转的基本块
    [[maybe_unused]] const auto remove_empty_blocks = [&]() -> void {
        const auto &blocks = func->get_blocks();
        auto modified{false};
        for (const auto &block: blocks) {
            if (block->is_deleted()) {
                continue;
            }
            const auto target = is_single_jump_block(block);
            if (target == nullptr || target->is_deleted()) {
                continue;
            }
            auto locked_users{block->users().lock()};
            const auto is_available = [&](const std::shared_ptr<User> &user) -> bool {
                if (const auto phi{user->is<Phi>()}) {
                    const auto &options{phi->get_optional_values()};
                    for (const auto &pre: predecessors.at(block)) {
                        if (options.find(pre) != options.end()) {
                            return false;
                        }
                    }
                }
                return true;
            };
            if (!std::all_of(locked_users.begin(), locked_users.end(), is_available)) {
                continue;
            }
            for (const auto &user: locked_users) {
                if (const auto phi = user->is<Phi>()) {
                    auto &options = phi->get_optional_values();
                    auto it = options.find(block);
                    if (it == options.end()) {
                        log_error("Phi operand not found");
                    }
                    const auto value = it->second;
                    for (const auto &pre: predecessors.at(block)) {
                        phi->set_optional_value(pre, value);
                        pre->add_user(phi);
                    }
                    phi->remove_optional_value(block);
                } else {
                    user->modify_operand(block, target);
                }
            }
            // 手动维护cfg
            for (const auto &pre: predecessors.at(block)) {
                successors.at(pre).erase(block);
                successors.at(pre).insert(target);
                predecessors.at(target).insert(pre);
            }
            predecessors.erase(block);
            successors.erase(block);
            predecessors.at(target).erase(block);

            block->get_instructions().clear();
            block->clear_operands();
            block->set_deleted();
            modified = true;
            graph_modified = true;
        }
        if (modified) {
            changed = true;
            remove_deleted_blocks(func);
        }
    };

    // 提升分支指令：消除只包含单个有条件跳转的基本块
    [[maybe_unused]] const auto hoist_branch = [&]() -> void {
        const auto &blocks = func->get_blocks();
        auto modified{false};
        for (const auto &block: blocks) {
            if (block->is_deleted()) {
                continue;
            }
            const auto branch = is_single_branch_block(block);
            if (branch == nullptr) {
                continue;
            }
            if (branch->get_true_block()->is_deleted() || branch->get_false_block()->is_deleted()) {
                continue;
            }
            const auto candidate = get_candidate_predecessors(block);
            if (candidate.size() != 1) {
                continue;
            }
            const auto candidate_block = *candidate.begin();
            auto locked_users{block->users().lock()};
            const auto is_available = [&](const std::shared_ptr<User> &user) -> bool {
                if (const auto phi{user->is<Phi>()}) {
                    const auto &options{phi->get_optional_values()};
                    for (const auto &pre: predecessors.at(block)) {
                        if (options.find(pre) != options.end()) {
                            return false;
                        }
                    }
                }
                return true;
            };
            if (!std::all_of(locked_users.begin(), locked_users.end(), is_available)) {
                continue;
            }
            const auto &last_instruction = candidate_block->get_instructions().back();
            last_instruction->clear_operands();
            candidate_block->get_instructions().pop_back();
            if (last_instruction->get_op() != Operator::JUMP) {
                log_error("last instruction should be a jump");
            }
            if (const auto jump = last_instruction->as<Jump>(); jump->get_target_block() != block) {
                log_error("jump target should be the block to be removed");
            }
            Branch::create(branch->get_cond(), branch->get_true_block(), branch->get_false_block(), candidate_block);
            block->replace_by_new_value(candidate_block);
            // 手动维护cfg
            for (const auto &pre: candidate) {
                successors.at(pre).erase(block);
                successors.at(pre).insert({branch->get_true_block(), branch->get_false_block()});
                predecessors.at(branch->get_true_block()).insert(pre);
                predecessors.at(branch->get_false_block()).insert(pre);
            }
            predecessors.erase(block);
            successors.erase(block);
            block->get_instructions().clear();
            block->clear_operands();
            block->set_deleted();
            modified = true;
            graph_modified = true;
        }
        if (modified) {
            changed = true;
            remove_deleted_blocks(func);
        }
    };

    [[maybe_unused]] const auto cleanup_switch = [&]() -> void {
        const auto &blocks = func->get_blocks();
        for (const auto &block: blocks) {
            if (block->is_deleted()) {
                continue;
            }
            if (block->get_instructions().back()->get_op() != Operator::SWITCH) {
                continue;
            }
            const auto switch_{block->get_instructions().back()->as<Switch>()};
            const auto default_block{switch_->get_default_block()};
            std::vector<std::shared_ptr<Const>> values;
            for (const auto &[value, block]: switch_->cases()) {
                if (block == default_block) {
                    values.push_back(value->as<Const>());
                }
            }
            std::for_each(values.begin(), values.end(), [&](const auto &value) { switch_->remove_case(value); });
        }
    };

    do {
        changed = false;
        fold_redundant_branch();
        combine_blocks();
        remove_empty_blocks();
        hoist_branch();
        cleanup_switch();
        if (changed) {
            remove_deleted_blocks(func);
        }
        try_constant_fold(func);
    } while (changed);

    if (graph_modified) {
        set_analysis_result_dirty<ControlFlowGraph>(func);
        set_analysis_result_dirty<DominanceGraph>(func);
    }
}

void SimplifyControlFlow::transform(const std::shared_ptr<Module> module) {
    // 预处理：清除不可达基本块
    for (const auto &func: module->get_functions()) {
        remove_unreachable_blocks(func);
    }

    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    cfg_info = get_analysis_result<ControlFlowGraph>(module);

    for (const auto &func: module->get_functions()) {
        cleanup_phi(func, cfg_info);
    }

    set_analysis_result_dirty<ControlFlowGraph>(module);
    cfg_info = nullptr;
}

void SimplifyControlFlow::transform(const std::shared_ptr<Function> &func) {
    remove_unreachable_blocks(func);
    cfg_info = get_analysis_result<ControlFlowGraph>(Module::instance());
    run_on_func(func);
    cfg_info = get_analysis_result<ControlFlowGraph>(Module::instance());
    cleanup_phi(func, cfg_info);
    set_analysis_result_dirty<ControlFlowGraph>(func);
    cfg_info = nullptr;
}
} // namespace Pass

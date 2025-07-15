#include <optional>
#include <type_traits>

#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Util.h"

using namespace Mir;

namespace {
template<typename Compare>
inline constexpr bool is_compare_v = std::is_same_v<Compare, Icmp> || std::is_same_v<Compare, Fcmp>;

template<typename>
struct always_false : std::false_type {};

template<typename Compare>
struct Trait {
    static_assert(always_false<Compare>::value, "Class Type is not a compare instruction");
};

template<>
struct Trait<Icmp> {
    using MaxInst = Smax;
    using MinInst = Smin;
};

template<>
struct Trait<Fcmp> {
    using MaxInst = FSmax;
    using MinInst = FSmin;
};
} // namespace

namespace {
// int max_value(int a, int b) {
//     if (a > b) {
//         return a;
//     } else {
//         return b;
//     }
// }
//
// int max_value(int a, int b) {
//     return max(a, b);  // 直接使用max指令
// }
template<typename Compare>
void select_handle(const std::shared_ptr<Block> &end_block, const std::shared_ptr<Block> &true_block,
                   const std::shared_ptr<Compare> &cmp) {
    using MaxInst = typename Trait<Compare>::MaxInst;
    using MinInst = typename Trait<Compare>::MinInst;

    const auto lhs{cmp->get_lhs()}, rhs{cmp->get_rhs()};
    std::unordered_set<std::shared_ptr<Instruction>> deleted_instructions;
    std::vector<std::shared_ptr<Instruction>> to_be_added_instructions;

    for (const auto &instruction: end_block->get_instructions()) {
        if (instruction->get_op() != Operator::PHI) [[unlikely]] {
            break;
        }
        const auto phi{instruction->as<Phi>()};
        if (std::any_of(phi->get_optional_values().begin(), phi->get_optional_values().end(),
                        [&](const auto &option) { return option.second != lhs && option.second != rhs; })) {
            continue;
        }
        deleted_instructions.insert(phi);
        const auto then_value{phi->get_optional_values()[true_block]};
        const auto new_inst = [&]() -> std::shared_ptr<Instruction> {
            switch (cmp->op) {
                case Compare::Op::LE:
                case Compare::Op::LT: {
                    if (then_value == lhs) {
                        return MinInst::create("min", lhs, rhs, end_block);
                    }
                    return MaxInst::create("max", lhs, rhs, end_block);
                }
                case Compare::Op::GE:
                case Compare::Op::GT: {
                    if (then_value == lhs) {
                        return MaxInst::create("max", lhs, rhs, end_block);
                    }
                    return MinInst::create("min", lhs, rhs, end_block);
                }
                default:
                    log_error("Invalid cmp instruction: %s", cmp->to_string().c_str());
            }
        }();
        to_be_added_instructions.push_back(new_inst);
        phi->replace_by_new_value(new_inst);
    }
    Pass::Utils::delete_instruction_set(Module::instance(), deleted_instructions);
    for (const auto &instruction: end_block->get_instructions()) {
        if (instruction->get_op() == Operator::PHI) {
            continue;
        }
        for (const auto &add: to_be_added_instructions) {
            Pass::Utils::move_instruction_before(add, instruction);
        }
        break;
    }
}

template<typename Compare>
void select_to_min_max(const std::shared_ptr<Function> &func, const std::shared_ptr<Pass::ControlFlowGraph> &cfg) {
    static_assert(is_compare_v<Compare>, "Class Type is not a compare instruction");
    std::unordered_set<std::shared_ptr<Block>> visited;
    for (const auto &block: func->get_blocks()) {
        if (visited.find(block) != visited.end()) {
            continue;
        }
        const auto &terminator{block->get_instructions().back()};
        if (terminator->get_op() != Operator::BRANCH) {
            continue;
        }
        const auto branch{terminator->as<Branch>()};
        auto true_block{branch->get_true_block()}, false_block{branch->get_false_block()};
        const auto compare{branch->get_cond()->is<Compare>()};
        if (compare == nullptr) {
            continue;
        }
        if (compare->op == Compare::Op::NE || compare->op == Compare::Op::EQ) {
            continue;
        }
        if (cfg->graph(func).predecessors.at(true_block).size() == 1 &&
            cfg->graph(func).predecessors.at(false_block).size() == 1) {
            if (true_block->get_instructions().back()->get_op() == Operator::JUMP &&
                false_block->get_instructions().back()->get_op() == Operator::JUMP) {
                const auto true_jump{true_block->get_instructions().back()->as<Jump>()},
                        false_jump{false_block->get_instructions().back()->as<Jump>()};
                if (true_jump->get_target_block() != false_jump->get_target_block()) {
                    continue;
                }
                visited.insert({true_block, false_block});
                const auto end_block{true_jump->get_target_block()};
                if (cfg->graph(func).predecessors.at(end_block).size() > 2) {
                    continue;
                }
                select_handle<Compare>(end_block, true_block, compare);
                if (std::none_of(end_block->get_instructions().begin(), end_block->get_instructions().end(),
                                 [&](const auto &inst) { return inst->get_op() == Operator::PHI; }) &&
                    true_block->get_instructions().size() == 1 && false_block->get_instructions().size() == 1) {
                    block->get_instructions().pop_back();
                    Jump::create(end_block, block);
                }
            }
        } else if (const auto flag{cfg->graph(func).predecessors.at(true_block).size() == 2};
                   flag || cfg->graph(func).predecessors.at(false_block).size() == 2) {
            const auto end_block{flag ? true_block : false_block}, pass_block{flag ? false_block : true_block};
            if (!cfg->graph(func).successors.at(pass_block).count(end_block)) {
                continue;
            }
            if (!cfg->graph(func).predecessors.at(end_block).count(pass_block)) {
                continue;
            }
            if (pass_block->get_instructions().back()->get_op() != Operator::JUMP) {
                continue;
            }
            visited.insert(pass_block);
            if (true_block == end_block) {
                true_block = block;
            }
            select_handle<Compare>(end_block, true_block, compare);
            if (std::none_of(end_block->get_instructions().begin(), end_block->get_instructions().end(),
                             [&](const auto &inst) { return inst->get_op() == Operator::PHI; }) &&
                pass_block->get_instructions().size() == 1) {
                block->get_instructions().pop_back();
                Jump::create(end_block, block);
            }
        }
    }
}


// 将连续出现的小于/小于等于或大于/大于等于转化为max min指令
// if (a < b) {
//     if (a < c) {
//         goto X;
//     } else {
//         goto Y;
//     }
// } else {
//     goto Y;
// }
// 转化为
// min_val = min(b, c);
// if (a < min_val) {
//     goto X;
// } else {
//     goto Y;
// }
bool _branch_to_min_max(const std::shared_ptr<Block> &block) {
    const auto is_valid_block = [&](const std::shared_ptr<Block> &b)
            -> std::optional<std::pair<std::shared_ptr<Branch>, std::shared_ptr<Icmp>>> {
        if (b->get_instructions().back()->get_op() != Operator::BRANCH) {
            return std::nullopt;
        }
        const auto br{b->get_instructions().back()->as<Branch>()};
        if (const auto icmp{br->get_cond()->is<Icmp>()}) {
            return std::nullopt;
        } else {
            return std::make_optional(std::pair{br, icmp});
        }
    };

    // target block中只能含有terminator或者是icmp
    const auto contains_single_jump = [&](const std::shared_ptr<Block> &b) -> bool {
        static const std::unordered_set valid_ops{Operator::RET, Operator::BRANCH, Operator::JUMP, Operator::ICMP};
        return std::all_of(b->get_instructions().begin(), b->get_instructions().end(),
                           [](const std::shared_ptr<Instruction> &instruction) {
                               return valid_ops.find(instruction->get_op()) != valid_ops.end();
                           });
    };

    const auto terminator{block->get_instructions().back()};
    if (terminator->get_op() != Operator::BRANCH) {
        return false;
    }
    const auto branch{terminator->as<Branch>()};
    const auto condition{branch->get_cond()};
    const auto icmp = condition->is<Icmp>();
    if (icmp == nullptr) {
        return false;
    }
    if (icmp->op == Icmp::Op::NE || icmp->op == Icmp::Op::EQ) {
        return false;
    }
    const auto a{icmp->get_lhs()}, b{icmp->get_rhs()};
    const auto true_block{branch->get_true_block()}, false_block{branch->get_false_block()};

    const auto try_convert = [&](const std::shared_ptr<Block> &target, const std::shared_ptr<Block> &other,
                                 const std::shared_ptr<Branch> &target_branch, const std::shared_ptr<Icmp> &target_icmp,
                                 const bool is_then) {
        if (!contains_single_jump(target)) {
            return;
        }
        if (is_then) {
            if (target_branch->get_true_block() == other) {
                target_icmp->reverse_op();
                target_branch->swap();
            }
            if (target_branch->get_false_block() != other) {
                return;
            }
        } else {
            if (target_branch->get_false_block() == other) {
                target_icmp->reverse_op();
                target_branch->swap();
            }
            if (target_branch->get_true_block() != other) {
                return;
            }
        }
        // ReSharper disable once CppTooWideScopeInitStatement
        const auto c{target_icmp->get_lhs()}, d{target_icmp->get_rhs()};
        if (a == d) {
            target_icmp->reverse_op();
        }
        if (b == c) {
            icmp->reverse_op();
        }
        if (icmp->get_lhs() != target_icmp->get_lhs() || icmp->op != target_icmp->op) {
            return;
        }
        const bool is_less{icmp->op == Icmp::Op::LE || icmp->op == Icmp::Op::LT};
        const auto cond_inst = [&]() -> std::shared_ptr<Instruction> {
            if ((is_then && is_less) || (!is_then && !is_less)) {
                return Smin::create("smin", icmp->get_rhs(), target_icmp->get_rhs(), icmp->get_block());
            }
            return Smax::create("smax", icmp->get_rhs(), target_icmp->get_rhs(), icmp->get_block());
        }();
        icmp->modify_operand(icmp->get_rhs(), cond_inst);
        Pass::Utils::move_instruction_before(cond_inst, icmp);
        // 删除branch
        block->get_instructions().pop_back();
        const auto target_true{target_branch->get_true_block()}, target_false{target_branch->get_false_block()};
        Branch::create(icmp, target_true, target_false, block);
    };

    if (const auto true_pair{is_valid_block(true_block)}; true_pair.has_value()) {
        try_convert(true_block, false_block, true_pair.value().first, true_pair.value().second, true);
        return true;
    }
    if (const auto false_pair{is_valid_block(false_block)}; false_pair.has_value()) {
        try_convert(false_block, true_block, false_pair.value().first, false_pair.value().second, false);
        return true;
    }
    return false;
}
} // namespace

namespace Pass {
void BranchMerging::run_on_func(const std::shared_ptr<Function> &func) {
    const auto refresh = [&]() -> void {
        func->update_id();
        SimplifyControlFlow::remove_unreachable_blocks(func);
        set_analysis_result_dirty<ControlFlowGraph>(func);
        set_analysis_result_dirty<DominanceGraph>(func);
        cfg_info = get_analysis_result<ControlFlowGraph>(Module::instance());
        dom_info = get_analysis_result<DominanceGraph>(Module::instance());
    };

    const auto branch_to_min_max = [&]() -> void {
        auto queue{dom_info->post_order_blocks(func)};
        std::reverse(queue.begin(), queue.end());
        std::unordered_set<std::shared_ptr<Block>> visited;

        while (!queue.empty()) {
            const auto block{queue.back()};
            queue.pop_back();
            if (visited.count(block)) {
                continue;
            }
            visited.insert(block);
            if (_branch_to_min_max(block)) {
                queue.insert(queue.begin(), block);
                visited.erase(block);
            }
        }
    };

    refresh();
    select_to_min_max<Icmp>(func, cfg_info);
    refresh();
    select_to_min_max<Fcmp>(func, cfg_info);
    refresh();
    branch_to_min_max();
    refresh();
}

void BranchMerging::transform(const std::shared_ptr<Module> module) {
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    dom_info = get_analysis_result<DominanceGraph>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    cfg_info = nullptr;
    dom_info = nullptr;
}
} // namespace Pass

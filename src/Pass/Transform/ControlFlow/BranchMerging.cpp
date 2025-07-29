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
        const auto then_value{phi->get_optional_values().at(true_block)};
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
// FIXME
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
[[deprecated, maybe_unused]]
bool _branch_to_min_max(const std::shared_ptr<Block> &block) {
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

    refresh();
    select_to_min_max<Icmp>(func, cfg_info);
    refresh();
    select_to_min_max<Fcmp>(func, cfg_info);
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

void BranchMerging::transform(const std::shared_ptr<Function> &func) {
    cfg_info = get_analysis_result<ControlFlowGraph>(Module::instance());
    dom_info = get_analysis_result<DominanceGraph>(Module::instance());
    run_on_func(func);
    cfg_info = nullptr;
    dom_info = nullptr;
}
} // namespace Pass

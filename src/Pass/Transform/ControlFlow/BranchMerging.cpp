#include <optional>

#include "Pass/Util.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
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
[[nodiscard]]
bool branch_to_min_max(const std::shared_ptr<Block> &block) {
    const auto is_valid_block = [&](const std::shared_ptr<Block> &b) ->
        std::optional<std::pair<std::shared_ptr<Branch>, std::shared_ptr<Icmp>>> {
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
}

namespace Pass {
void BranchMerging::run_on_func(const std::shared_ptr<Function> &func) const {
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
        if (branch_to_min_max(block)) {
            queue.insert(queue.begin(), block);
            visited.erase(block);
        }
    }

    set_analysis_result_dirty<DominanceGraph>(func);
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
}

#include <map>
#include "Pass/Transform.h"
using namespace Mir;

namespace {
[[maybe_unused]]
void build_case_map(const std::shared_ptr<Value> &base,
                    const std::shared_ptr<Block> &current_block, const bool head,
                    std::map<int, std::shared_ptr<Block>> &switch_map,
                    std::unordered_set<std::shared_ptr<Block>> &visited,
                    std::shared_ptr<Block> &default_block, std::shared_ptr<Block> &parent_block) {
    visited.insert(current_block);
    if (!head) {
        const auto terminator = current_block->get_instructions().back();
    }
}
}

namespace Pass {
void If2Switch::run_on_func(const std::shared_ptr<Function> &func) {
    auto is_valid_icmp = [&](const std::shared_ptr<Instruction> &instruction) -> std::shared_ptr<Icmp> {
        if (instruction->get_op() != Operator::BRANCH) {
            return nullptr;
        }
        const auto cond_branch = instruction->as<Branch>();
        const auto cmp = cond_branch->get_cond()->as<Instruction>();
        if (cmp->get_op() != Operator::ICMP) {
            return nullptr;
        }
        auto icmp = cmp->as<Icmp>();
        if (const auto op = icmp->op; op != Icmp::Op::EQ && op != Icmp::Op::NE) {
            return nullptr;
        }
        if (!icmp->get_rhs()->is_constant()) {
            return nullptr;
        }
        return icmp;
    };

    const auto blocks = dom_info->dom_tree_layer(func);
    std::unordered_set<std::shared_ptr<Block>> visited{};
    for (const auto &block: blocks) {
        if (visited.count(block)) {
            continue;
        }
        const auto terminator = block->get_instructions().back();
        if (terminator->is<Terminator>() == nullptr) {
            log_error("Last instruction of %s is not a terminator: %s",
                      block->get_name().c_str(), terminator->to_string().c_str());
        }
        if (terminator->get_op() != Operator::BRANCH) {
            continue;
        }
        const auto icmp = is_valid_icmp(terminator);
        if (icmp == nullptr) {
            continue;
        }

        std::shared_ptr<Block> default_block{nullptr}, parent_block{nullptr};
        std::map<int, std::shared_ptr<Block>> switch_map{};
    }
}

void If2Switch::transform(const std::shared_ptr<Module> module) {
    create<StandardizeBinary>()->run_on(module);
    dom_info = get_analysis_result<DominanceGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
    dom_info = nullptr;
}
}

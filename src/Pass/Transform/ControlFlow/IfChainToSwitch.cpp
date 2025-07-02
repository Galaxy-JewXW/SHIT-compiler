#include <optional>

#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
void run_on_block(const std::shared_ptr<Block> &block, std::unordered_set<std::shared_ptr<Block>> &visited) {
    const auto &terminator{block->get_instructions().back()};
    if (terminator->get_op() != Operator::BRANCH)
        return;
    const auto branch{terminator->as<Branch>()};
    const auto icmp{branch->get_cond()->is<Icmp>()};
    if (!icmp)
        return;
    if (icmp->op != Icmp::Op::EQ && icmp->op != Icmp::Op::NE)
        return;
    if (icmp->get_lhs()->is_constant() || !icmp->get_rhs()->is_constant())
        return;

    const auto base_value{icmp->get_lhs()};
    std::shared_ptr<Block> default_block{nullptr}, parent_block{nullptr};
    std::unordered_map<int, std::shared_ptr<Block>> chain_map;

    [[maybe_unused]] const auto make_chain = [&](auto &&self, decltype(block) cur_block, const bool is_head) -> void {
        /**
        * A: if (x == 1) goto B else goto C
        * C: if (x == 2) goto D else goto E
        * E: if (x == 3) goto F else goto G
        * G: [others]
        * 从块A开始，匹配x == 1，添加映射 1 -> B
        * 递归处理块C，匹配x == 2，添加映射 2 -> D
        * 递归处理块E，匹配x == 3，添加映射 3 -> F
        * 递归处理块G，无匹配，将G设为defaultBlock
        * 最终构建映射 {1:B, 2:D, 3:F} 和默认目标G
         */
        visited.insert(cur_block);
        const auto &_insts{cur_block->get_instructions()};
        if (!is_head) {
            // 确保中间块只包含比较逻辑，没有其他副作用
            // 若块中有指令被 terminator 以外的指令使用，则认为该块是default目标
            const auto &_terminator{_insts.back()};
            const bool used_by_other = std::any_of(_insts.begin(), _insts.end(), [&](const auto &inst) {
                const auto users{inst->users().lock()};
                return std::any_of(users.begin(), users.end(), [&](const auto &user) { return user != _terminator; });
            });
            if (used_by_other) {
                default_block = cur_block;
                return;
            }
        }

        std::optional<int> key{std::nullopt};
        std::shared_ptr<Block> then_block, else_block;

        const auto match_icmp = [&]() -> bool {
            const auto &_terminator{_insts.back()};
            if (_terminator->get_op() != Operator::BRANCH)
                return false;
            const auto _branch{_terminator->as<Branch>()};
            const auto _icmp{_branch->get_cond()->is<Icmp>()};
            if (!_icmp)
                return false;
            if (_icmp->op != Icmp::Op::EQ && _icmp->op != Icmp::Op::NE)
                return false;
            if (_icmp->get_lhs()->is_constant() || !_icmp->get_rhs()->is_constant())
                return false;
            if (_icmp->get_lhs() != base_value)
                return false;
            key = std::make_optional(**_icmp->get_rhs()->as<ConstInt>());
            if (_icmp->op == Icmp::Op::EQ) {
                then_block = _branch->get_true_block();
                else_block = _branch->get_false_block();
                return true;
            }
            if (_icmp->op == Icmp::Op::NE) {
                then_block = _branch->get_false_block();
                else_block = _branch->get_true_block();
                return true;
            }
            return false;
        };

        // 如果不匹配，将当前块设为default目标
        if (match_icmp() && key && chain_map.find(key.value()) == chain_map.end()) {
            chain_map.insert({key.value(), then_block});
            parent_block = cur_block;
            self(self, else_block, false);
        } else {
            default_block = cur_block;
        }
    };

    make_chain(make_chain, block, true);

    log_info("switch: %s", base_value->to_string().c_str());
    for (const auto &[key, block]: chain_map) {
        log_info("  %d -> %s", key, block->get_name().c_str());
    }
    log_info("  default: %s", default_block->get_name().c_str());

    if (chain_map.size() <= 1)
        return;

    if (std::any_of(chain_map.begin(), chain_map.end(), [](const auto &pair) {
        return pair.second->get_instructions().front()->get_op() == Operator::PHI;
    })) {
        return;
    }
    block->get_instructions().pop_back();
    const auto switch_{Switch::create(base_value, default_block, block)};
    for (const auto &inst: default_block->get_instructions()) {
        if (inst->get_op() == Operator::PHI) {
            const auto phi{inst->as<Phi>()};
            phi->set_optional_value(block, phi->get_optional_values()[parent_block]);
        } else {
            break;
        }
    }
    std::for_each(chain_map.begin(), chain_map.end(), [&](const auto &pair) {
        switch_->set_case(ConstInt::create(pair.first), pair.second);
    });
}
}

namespace Pass {
void IfChainToSwitch::run_on_func(const std::shared_ptr<Function> &func) const {
    std::unordered_set<std::shared_ptr<Block>> visited;
    const auto pre_order_blocks{dom_info->pre_order_blocks(func)};
    for (const auto &block: pre_order_blocks) {
        if (visited.find(block) != visited.end())
            continue;
        run_on_block(block, visited);
    }
}

void IfChainToSwitch::transform(const std::shared_ptr<Module> module) {
    create<StandardizeBinary>()->run_on(module);
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    dom_info = get_analysis_result<DominanceGraph>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    cfg_info = nullptr;
    dom_info = nullptr;
}
}

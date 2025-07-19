#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transforms/Loop.h"

namespace Pass {
void LCSSA::transform(std::shared_ptr<Mir::Module> module) {
    const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);
    const auto dom_info = get_analysis_result<DominanceGraph>(module);
    const auto loop_info = get_analysis_result<LoopAnalysis>(module);
    this->set_cfg(cfg_info);
    this->set_loop_info(loop_info);


    for (auto &fun: *module) {
        this->run_on(fun);
    }
}

void LCSSA::runOnNode(const std::shared_ptr<LoopNodeTreeNode> &loop_node) {
    for (auto &child: loop_node->get_children())
        runOnNode(child);

    for (auto &block: loop_node->get_loop()->get_blocks()) {
        for (auto &inst: block->get_instructions()) {
            auto loop = loop_node->get_loop();
            if (usedOutLoop(inst, loop)) {
                for (auto &exit: loop->get_exits())
                    addPhi4Exit(inst, exit, loop);
            }
        }
    }
}

void LCSSA::addPhi4Exit(const std::shared_ptr<Mir::Instruction> &inst, const std::shared_ptr<Mir::Block> &exit,
                        const std::shared_ptr<Loop> &loop) {

    auto new_phi = Mir::Phi::create("phi", inst->get_type(), nullptr, {});
    new_phi->set_block(exit, false);
    exit->get_instructions().insert(exit->get_instructions().begin(), new_phi);

    auto block_pre = this->cfg_info()->graph(exit->get_function()).predecessors;
    for (auto &pre: block_pre[exit]) {
        new_phi->set_optional_value(pre, inst);
    }

    std::vector<std::shared_ptr<Mir::Instruction>> out_user;
    auto block_dominated = this->dom_info()->graph(exit->get_function()).dominated_blocks;
    auto dominated = block_dominated[exit];
    for (auto user: inst->users()) {
        if (auto user_instr = std::dynamic_pointer_cast<Mir::Instruction>(user)) {
            if (loop->contain_block(user_instr->get_block()))
                continue;
            if (user_instr->get_op() == Mir::Operator::PHI) {
                if (std::find(loop->get_exits().begin(), loop->get_exits().end(), user_instr->get_block()) !=
                    loop->get_exits().end())
                    continue;

                auto phi_user = std::dynamic_pointer_cast<Mir::Phi>(user_instr);
                auto coming_block = phi_user->find_optional_block(inst);
                if (std::find(dominated.begin(), dominated.end(), coming_block) == dominated.end())
                    continue;
            } else {
                if (std::find(dominated.begin(), dominated.end(), user_instr->get_block()) == dominated.end())
                    continue;
            }
            out_user.push_back(user_instr);
        }
    }

    for (const auto &user: out_user) {
        user->modify_operand(inst, new_phi);
    }
}

bool LCSSA::usedOutLoop(const std::shared_ptr<Mir::Instruction> &inst, const std::shared_ptr<Loop> &loop) {
    for (auto user: inst->users()) {
        if (auto user_instr = std::dynamic_pointer_cast<Mir::Instruction>(user)) {
            auto parent_block = user_instr->get_block();
            if (!loop->contain_block(parent_block))
                return true;
        }
    }
    return false;
}

void LCSSA::transform(const std::shared_ptr<Mir::Function> & func) {
    auto module = Mir::Module::instance();
    const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);
    const auto dom_info = get_analysis_result<DominanceGraph>(module);
    const auto loop_info = get_analysis_result<LoopAnalysis>(module);
    this->set_cfg(cfg_info);
    this->set_loop_info(loop_info);

    for (const auto& loop_node : loop_info->loop_forest(func)) {
        runOnNode(loop_node);
    }
}
} // namespace Pass

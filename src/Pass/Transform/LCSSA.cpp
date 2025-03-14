#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transform.h"

namespace Pass {

void LCSSA::transform(std::shared_ptr<Mir::Module> module) {
    const auto cfg_info = create<ControlFlowGraph>();
    const auto loop_info = create<LoopAnalysis>();
    cfg_info->run_on(module);
    loop_info->run_on(module);
    this->set_cfg(cfg_info);

    for (auto &fun: *module) {
        for (const auto& loop_node: loop_info->loop_forest(fun)) {
            runOnNode(loop_node);
        }
    }
}

void LCSSA::runOnNode(std::shared_ptr<LoopNodeTreeNode> loop_node) {
    for (auto& child : loop_node->get_children()) runOnNode(child);

    for (auto& block : loop_node->get_loop()->get_blocks()) {
        for (auto& inst : block->get_instructions()) {
            auto loop = loop_node->get_loop();
            if (usedOutLoop(inst, loop)) {
                for (auto& exit : loop->get_exits())
                addPhi4Exit(inst, exit, loop);
            }
        }
    }
}

void LCSSA::addPhi4Exit(std::shared_ptr<Mir::Instruction> inst, std::shared_ptr<Mir::Block> exit, std::shared_ptr<Loop> loop) {
    Mir::Phi::Optional_Values values;
    auto new_phi = Mir::Phi::create("phi", inst->get_type(), nullptr, values);
    new_phi->set_block(exit, false);
    exit->get_instructions().insert(exit->get_instructions().begin(), new_phi);

    auto block_pre = this->cfg_info()->predecessors(exit->get_function());
    for (auto& pre : block_pre[exit]) {
        new_phi->set_optional_value(pre, inst);
    }

    std::vector<std::shared_ptr<Mir::Instruction>> out_user;
    auto block_dominated = this->cfg_info()->dominated(exit->get_function());
    auto dominated = block_dominated[exit];
    for (auto user : inst->users()) {
        if (auto user_instr = std::dynamic_pointer_cast<Mir::Instruction>(user)) {
            if (loop->contain_block(user_instr->get_block())) continue;
            if (user_instr->get_op() == Mir::Operator::PHI) {
                if (std::find(loop->get_exits().begin(), loop->get_exits().end(), user_instr->get_block()) != loop->get_exits().end())
                    continue;

                auto phi_user = std::dynamic_pointer_cast<Mir::Phi>(user_instr);
                auto coming_block = phi_user->find_optional_block(inst);
                if (std::find(dominated.begin(), dominated.end(), coming_block) == dominated.end())
                    continue;
            }
            else {
                if (std::find(dominated.begin(), dominated.end(), user_instr->get_block()) == dominated.end())
                    continue;
            }
            out_user.push_back(user_instr);
        }
    }

    for (auto user: out_user) {
        user->modify_operand(inst, new_phi);
    }

}

bool LCSSA::usedOutLoop(std::shared_ptr<Mir::Instruction> inst, std::shared_ptr<Loop> loop) {
    for (auto user : inst->users()) {
        if (auto user_instr = std::dynamic_pointer_cast<Mir::Instruction>(user)) {
            auto parent_block = user_instr->get_block();
            if (!loop->contain_block(parent_block)) return true;
        }
    }
    return false;
}
}

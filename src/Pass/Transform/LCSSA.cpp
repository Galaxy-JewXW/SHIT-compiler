#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transform.h"

namespace Pass {

void LCSSA::transform(std::shared_ptr<Mir::Module> module) {
    const auto loop_info = create<LoopAnalysis>();
    loop_info->run_on(module);
    for (auto &fun: *module) {
        for (auto loop_node: loop_info->loop_forest(fun)) {
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

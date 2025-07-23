#ifndef FUNCTIONCLONEHELPER_H
#define FUNCTIONCLONEHELPER_H

#include "Instruction.h"

namespace Pass {
class ControlFlowGraph;
}

namespace Mir {
class FunctionCloneHelper {
public:
    explicit FunctionCloneHelper();

    std::shared_ptr<Function> clone_function(const std::shared_ptr<Function> &origin_func);

    std::shared_ptr<Value> get_or_create(const std::shared_ptr<Value> &origin_value);

private:
    std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> value_map;

    std::unordered_set<std::shared_ptr<Block>> visited_blocks;

    std::unordered_set<std::shared_ptr<Phi>> phis;

    std::unordered_set<std::shared_ptr<Instruction>> cloned_instructions;

    std::shared_ptr<Pass::ControlFlowGraph> cfg_info{nullptr};

    std::shared_ptr<Function> current_func{nullptr};

    void clone_instruction(const std::shared_ptr<Instruction> &origin_instruction,
                           const std::shared_ptr<Block> &parent_block, bool lazy_cloning);

    void clone_block(const std::shared_ptr<Block> &origin_block);

public:
    std::unordered_set<std::shared_ptr<Phi>> &get_phis() { return phis; }
};
}

#endif //FUNCTIONCLONEHELPER_H

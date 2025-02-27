#include "Pass/Analysis.h"
#include "Pass/Transform.h"

using namespace Mir;

static std::shared_ptr<Pass::FunctionAnalysis> func_graph = nullptr;
using InstructionPtr = std::shared_ptr<Instruction>;

namespace Pass {
// 删除无用指令
// 如果指令User为空，且指令本身不带有副作用，则认为其是无用的
// TODO: 效果较差，无法删除冗余数组的定义，需要考虑更好的判断方法
static void remove_unused_instructions(const std::shared_ptr<Module> &module) {
    auto is_dead_instruction = [](const InstructionPtr &instruction) -> bool {
        if (instruction->users().size() > 0) { return false; }
        // instruction无返回值
        if (instruction->get_name().empty()) { return false; }
        if (instruction->get_op() == Operator::CALL) {
            const auto &call_inst = std::static_pointer_cast<Call>(instruction);
            const auto &called_func = std::static_pointer_cast<Function>(call_inst->get_function());
            if (called_func->is_runtime_func()) { return false; }
            if (func_graph->has_side_effect(called_func)) { return false; }
            return true;
        }
        return true;
    };
    for (const auto &func: *module) {
        for (const auto &block: func->get_blocks()) {
            for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
                if (is_dead_instruction(*it)) {
                    (*it)->clear_operands();
                    it = block->get_instructions().erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}

void DeadInstEliminate::transform(const std::shared_ptr<Module> module) {
    func_graph = create<FunctionAnalysis>();
    func_graph->run_on(module);
    remove_unused_instructions(module);
    func_graph = nullptr;
}
}

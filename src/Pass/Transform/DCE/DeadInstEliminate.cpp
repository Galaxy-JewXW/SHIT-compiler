#include "Pass/Analysis.h"
#include "Pass/Transforms/DCE.h"

using namespace Mir;
using InstructionPtr = std::shared_ptr<Instruction>;

namespace Pass {
// 删除无用指令
// 如果指令User为空，且指令本身不带有副作用，则认为其是无用的
// 效果较差，无法删除冗余数组的定义，可使用DCE取得更好的效果
bool DeadInstEliminate::remove_unused_instructions(const std::shared_ptr<Module> &module) const {
    auto is_dead_instruction = [&](const InstructionPtr &instruction) -> bool {
        if (instruction->users().size() > 0) { return false; }
        // instruction无返回值
        if (instruction->get_name().empty()) { return false; }
        if (instruction->get_op() == Operator::CALL) {
            const auto &call_inst = std::static_pointer_cast<Call>(instruction);
            const auto &called_func = std::static_pointer_cast<Function>(call_inst->get_function());
            if (called_func->is_runtime_func()) { return false; }
            if (const auto info = func_analysis->func_info(called_func);
                info.io_read || info.io_write || info.memory_read ||
                info.memory_read || info.has_side_effect || !info.no_state) {
                return false;
            }
            return true;
        }
        return true;
    };
    bool changed = false;
    for (const auto &func: *module) {
        for (const auto &block: func->get_blocks()) {
            for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
                if (is_dead_instruction(*it)) {
                    (*it)->clear_operands();
                    it = block->get_instructions().erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }
        }
    }
    return changed;
}

void DeadInstEliminate::transform(const std::shared_ptr<Module> module) {
    func_analysis = create<FunctionAnalysis>();
    func_analysis->run_on(module);
    while (remove_unused_instructions(module)) {
        func_analysis = create<FunctionAnalysis>();
        func_analysis->run_on(module);
    }
    func_analysis = nullptr;
}
}

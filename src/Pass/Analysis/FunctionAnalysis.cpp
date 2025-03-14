#include "Mir/Instruction.h"
#include "Pass/Analysis.h"

using FunctionPtr = std::shared_ptr<Mir::Function>;
using FunctionMap = std::unordered_map<FunctionPtr, std::unordered_set<FunctionPtr>>;
using FunctionSet = std::unordered_set<FunctionPtr>;

static void build_call_graph(const FunctionPtr &function, FunctionMap &call_map, FunctionMap &call_map_reverse) {
    for (const auto &block: function->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() != Mir::Operator::CALL) [[likely]] {
                continue;
            }
            const auto call = std::static_pointer_cast<Mir::Call>(inst);
            const auto called_function = std::static_pointer_cast<Mir::Function>(call->get_function());
            if (called_function->is_runtime_func()) [[likely]] {
                continue;
            }
            call_map[function].insert(called_function);
            call_map_reverse[called_function].insert(function);
        }
    }
}

void Pass::FunctionAnalysis::analyze(const std::shared_ptr<const Mir::Module> module) {
    // 构建函数调用图
    for (const auto &func: *module) {
        build_call_graph(func, call_graph_, call_graph_reverse_);
    }
}

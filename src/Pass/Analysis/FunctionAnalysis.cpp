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
    for (const auto &func: *module) {
        build_call_graph(func, call_graph_, call_graph_reverse_);
    }
    for (const auto &func: *module) {
        std::ostringstream log_msg;
        log_msg << "\nFunction [" << func->get_name() << "] calls:";
        if (call_graph_.find(func) == call_graph_.end()) {
            log_msg << "\n  No callees";
        } else {
            for (const auto &callee: call_graph_[func]) {
                log_msg << "\n  - " << callee->get_name();
            }
        }
        log_msg << "\nFunction [" << func->get_name() << "] is called by:";
        if (call_graph_reverse_.find(func) == call_graph_reverse_.end()) {
            log_msg << "\n  No callers";
        } else {
            for (const auto &caller: call_graph_reverse_[func]) {
                log_msg << "\n  - " << caller->get_name();
            }
        }
        log_debug(log_msg.str().c_str());
    }
}

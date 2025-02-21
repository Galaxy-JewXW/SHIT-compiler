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

static bool analyse_side_effect(const FunctionPtr &function) {
    using namespace Mir;
    for (const auto &block: function->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (const auto op = inst->get_op(); op == Operator::CALL) {
                const auto &call = std::static_pointer_cast<Call>(inst);
                if (const auto &called_func = std::static_pointer_cast<Function>(call->get_function());
                    called_func->is_sysy_runtime_func()) {
                    return true;
                }
            } else if (op == Operator::STORE) {
                const auto &store = std::static_pointer_cast<Store>(inst);
                const auto &addr = store->get_addr();
                if (std::dynamic_pointer_cast<GlobalVariable>(addr)) {
                    return true;
                }
                if (auto gep = std::dynamic_pointer_cast<GetElementPtr>(addr)) {
                    std::shared_ptr<Value> current_address = gep;
                    while (const auto cur_gep = std::dynamic_pointer_cast<GetElementPtr>(current_address)) {
                        current_address = cur_gep->get_addr();
                    }
                    // TODO：这里需要考虑全局变量是否为常值
                    if (const auto &gv = std::dynamic_pointer_cast<GlobalVariable>(current_address)) {
                        if (!gv->is_constant_gv()) return true;
                    }
                    if (std::dynamic_pointer_cast<Argument>(current_address)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static void analyse_io(const FunctionPtr &function, FunctionSet &input, FunctionSet &output) {
    using namespace Mir;
    for (const auto &block: function->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() != Operator::CALL) continue;
            const auto &call = std::static_pointer_cast<Call>(inst);
            const auto &called_func = std::static_pointer_cast<Function>(call->get_function());
            const auto &func_name = called_func->get_name();
            if (func_name.find("get") != std::string::npos) {
                input.insert(function);
            }
            if (func_name.find("put") != std::string::npos) {
                output.insert(function);
            }
        }
    }
}

void Pass::FunctionAnalysis::analyze(const std::shared_ptr<const Mir::Module> module) {
    clear();
    // 构建函数调用图
    for (const auto &func: *module) {
        build_call_graph(func, call_graph_, call_graph_reverse_);
    }
    // 确定函数是否有副作用
    for (const auto &func: *module) {
        if (analyse_side_effect(func)) {
            side_effect_functions_.insert(func);
        }
    }
    // 传播副作用
    bool changed = false;
    do {
        for (const auto &[func, callees]: call_graph_) {
            bool has_side_effect = false;
            for (const auto &callee: callees) {
                if (side_effect_functions_.find(callee) != side_effect_functions_.end()) {
                    has_side_effect = true;
                    break;
                }
            }
            if (has_side_effect) {
                changed = true;
                side_effect_functions_.insert(func);
            }
        }
    } while (changed);
    // 分析函数是否接受输入/返回输出
    for (const auto &func: *module) {
        analyse_io(func, accept_input_functions_, return_output_functions_);
    }
    for (const auto &func: *module) {
        std::ostringstream log_msg;
        log_msg << "\n";
        if (side_effect_functions_.find(func) != side_effect_functions_.end()) {
            log_msg << "[Side Effect] ";
        }
        if (accept_input_functions_.find(func) != accept_input_functions_.end()) {
            log_msg << "[I] ";
        }
        if (return_output_functions_.find(func) != return_output_functions_.end()) {
            log_msg << "[O] ";
        }
        log_msg << "Function [" << func->get_name() << "] calls:";
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

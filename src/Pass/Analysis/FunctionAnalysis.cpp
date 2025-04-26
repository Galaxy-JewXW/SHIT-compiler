#include <functional>

#include "Pass/Analyses/FunctionAnalysis.h"
#include "Mir/Instruction.h"

using FunctionPtr = std::shared_ptr<Mir::Function>;
using FunctionMap = std::unordered_map<FunctionPtr, std::unordered_set<FunctionPtr>>;
using FunctionSet = std::unordered_set<FunctionPtr>;

namespace {
// 是否使用了全局变量的地址
bool use_global_addr(const std::shared_ptr<Mir::Value> &base_addr) {
    if (std::dynamic_pointer_cast<Mir::GlobalVariable>(base_addr)) {
        return true;
    }
    if (const auto gep = std::dynamic_pointer_cast<Mir::GetElementPtr>(base_addr);
        gep != nullptr && use_global_addr(gep->get_addr())) {
        return true;
    }
    return false;
}

// 是否对传入的指针（数组参数）进行写操作
bool has_side_effect(std::shared_ptr<Mir::Value> addr) {
    while (const auto gep = std::dynamic_pointer_cast<Mir::GetElementPtr>(addr)) {
        addr = gep->get_addr();
    }
    return std::dynamic_pointer_cast<Mir::Argument>(addr) != nullptr;
}

std::vector<FunctionPtr> topo_order(const FunctionPtr &main, const FunctionMap &call_graph) {
    std::unordered_set<FunctionPtr> visited;
    std::vector<FunctionPtr> order;
    std::function<void(const FunctionPtr &)> dfs = [&](const FunctionPtr &func) {
        visited.insert(func);
        if (call_graph.find(func) != call_graph.end()) {
            for (const auto &called: call_graph.at(func)) {
                if (visited.find(called) == visited.end()) {
                    dfs(called);
                }
            }
        }
        order.push_back(func);
    };
    dfs(main);
    return order;
}
}

namespace Pass {
void FunctionAnalysis::build_call_graph(const FunctionPtr &func) {
    for (const auto &block: func->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() != Mir::Operator::CALL) [[likely]] {
                continue;
            }
            const auto call = inst->as<Mir::Call>();
            const auto called_function = call->get_function()->as<Mir::Function>();
            if (called_function->is_runtime_func()) [[likely]] {
                continue;
            }
            call_graph_[func].insert(called_function);
            call_graph_reverse_[called_function].insert(func);
        }
    }
}

void FunctionAnalysis::build_func_attribute(const FunctionPtr &func) {
    bool memory_alloc{false}, memory_write{false}, memory_read{false};
    bool io_read{false}, io_write{false};
    bool side_effect{false}, rely_on_state{false};
    for (const auto &block: func->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() == Mir::Operator::LOAD) {
                const auto &load = inst->as<Mir::Load>();
                if (use_global_addr(load->get_addr())) {
                    infos_[func].used_global_variables.insert(load->get_addr()->as<Mir::GlobalVariable>());
                    memory_read = true;
                }
                rely_on_state |= has_side_effect(load);
            } else if (inst->get_op() == Mir::Operator::STORE) {
                const auto &store = inst->as<Mir::Store>();
                if (use_global_addr(store->get_addr())) {
                    infos_[func].used_global_variables.insert(store->get_addr()->as<Mir::GlobalVariable>());
                    memory_write = true;
                }
                side_effect |= has_side_effect(store->get_addr());
            } else if (inst->get_op() == Mir::Operator::ALLOC) {
                memory_alloc = true;
            } else if (inst->get_op() == Mir::Operator::CALL) {
                const auto &call = inst->as<Mir::Call>();
                if (const auto &called_func = call->get_function()->as<Mir::Function>();
                    called_func->is_runtime_func()) {
                    if (const auto name = called_func->get_name();
                        name.find("get") != std::string::npos) {
                        io_read = true;
                    } else if (name.find("put") != std::string::npos) {
                        io_write = true;
                    } else if (name.find("time") != std::string::npos) {
                        io_read = true;
                    }
                    // memset只在栈内存分配时被使用，暂不认为会带来全局内存读写or产生副作用
                }
            }
        }
    }
    // 有形参是数组指针，则函数依赖于传入的状态
    for (const auto &arg: func->get_arguments()) {
        if (arg->get_type()->is_pointer()) {
            rely_on_state = true;
            break;
        }
    }
    infos_[func].memory_read = memory_read;
    infos_[func].memory_write = memory_write;
    infos_[func].memory_alloc = memory_alloc;
    infos_[func].io_read = io_read;
    infos_[func].io_write = io_write;
    infos_[func].has_return = !func->get_return_type()->is_void();
    infos_[func].has_side_effect = side_effect;
    infos_[func].no_state = !rely_on_state && !memory_read && !memory_write && !side_effect;
}

void FunctionAnalysis::transmit_attribute(const std::vector<FunctionPtr> &topo) {
    for (const auto &func: topo) {
        for (const auto &callee: call_graph_.at(func)) {
            infos_[func].memory_read |= infos_[callee].memory_read;
            infos_[func].memory_write |= infos_[callee].memory_write;
            infos_[func].memory_alloc |= infos_[callee].memory_alloc;
            infos_[func].io_read |= infos_[callee].io_read;
            infos_[func].io_write |= infos_[callee].io_write;
            infos_[func].has_side_effect |= infos_[callee].has_side_effect;
            // infos_[func].used_global_variables;
            std::set_union(infos_[func].used_global_variables.begin(), infos_[func].used_global_variables.end(),
                           infos_[callee].used_global_variables.begin(), infos_[callee].used_global_variables.end(),
                           std::inserter(infos_[func].used_global_variables,
                                         infos_[func].used_global_variables.begin()));
        }
        infos_[func].no_state = infos_[func].no_state && !infos_[func].has_side_effect &&
                                !infos_[func].memory_read && !infos_[func].memory_write;
    }
}

static void print_function_analysis(const FunctionPtr &func, const FunctionMap &call_graph,
                                    const FunctionMap &call_graph_reverse,
                                    const std::unordered_map<FunctionPtr, FunctionAnalysis::FunctionInfo> &infos) {
    std::ostringstream oss;
    oss << "Function: " << func->get_name() << std::endl;
    oss << "Functions called by " << func->get_name() << ":" << std::endl;
    if (const auto &callees = call_graph.at(func); !callees.empty()) {
        for (const auto &callee: callees) {
            oss << "  - " << callee->get_name() << std::endl;
        }
    } else {
        oss << "  (None)" << std::endl;
    }
    oss << "Functions that call " << func->get_name() << ":" << std::endl;
    if (const auto &callers = call_graph_reverse.at(func); !callers.empty()) {
        for (const auto &caller: callers) {
            oss << "  - " << caller->get_name() << std::endl;
        }
    } else {
        oss << "  (None)" << std::endl;
    }
    const auto &[is_recursive, is_leaf, memory_read, memory_write,
        memory_alloc, io_read, io_write, has_return,
        has_side_effect, no_state, used_global_variables] = infos.at(func);
    oss << "Function attributes:" << std::endl;
    oss << "  is_recursive    : " << (is_recursive ? "true" : "false") << std::endl;
    oss << "  is_leaf         : " << (is_leaf ? "true" : "false") << std::endl;
    oss << "  memory_read     : " << (memory_read ? "true" : "false") << std::endl;
    oss << "  memory_write    : " << (memory_write ? "true" : "false") << std::endl;
    oss << "  memory_alloc    : " << (memory_alloc ? "true" : "false") << std::endl;
    oss << "  io_read         : " << (io_read ? "true" : "false") << std::endl;
    oss << "  io_write        : " << (io_write ? "true" : "false") << std::endl;
    oss << "  has_return      : " << (has_return ? "true" : "false") << std::endl;
    oss << "  has_side_effect : " << (has_side_effect ? "true" : "false") << std::endl;
    oss << "  no_state        : " << (no_state ? "true" : "false") << std::endl;
    log_debug("\n%s", oss.str().c_str());
}

void FunctionAnalysis::analyze(const std::shared_ptr<const Mir::Module> module) {
    for (const auto &func: *module) {
        call_graph_[func] = {};
        call_graph_reverse_[func] = {};
        infos_[func] = {};
    }
    for (const auto &func: *module) {
        build_call_graph(func);
        infos_[func].is_leaf = call_graph_[func].empty();
        if (call_graph_[func].count(func)) {
            infos_[func].is_recursive = true;
        }
    }
    for (const auto &func: *module) {
        build_func_attribute(func);
    }
    topo_ = topo_order(module->get_main_function(), call_graph_);
    transmit_attribute(topo_);
    for (const auto &func: *module) {
        print_function_analysis(func, call_graph_, call_graph_reverse_, infos_);
    }
}
}

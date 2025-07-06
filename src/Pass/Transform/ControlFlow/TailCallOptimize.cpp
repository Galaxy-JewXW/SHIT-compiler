#include <unordered_set>

#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Util.h"

using namespace Mir;

namespace {
// 检查从 call 到 end_block 的所有路径是否都不含栈访问
// 如果安全，返回 true；否则返回 false
bool path_without_stack_access(const std::shared_ptr<Call> &call, const std::shared_ptr<Block> &end_block,
                               const std::unordered_set<std::shared_ptr<Value>> &stack_allocs,
                               const Pass::ControlFlowGraph::Graph &cfg) {
    const auto start_block{call->get_block()};
    std::unordered_set<std::shared_ptr<Block>> visited;

    const auto access_stack = [&stack_allocs](auto &&self, const std::shared_ptr<Value> &value) -> bool {
        if (stack_allocs.count(value)) {
            return true;
        }
        if (const auto gep{value->is<GetElementPtr>()}) {
            return self(self, gep->get_addr());
        }
        if (const auto bitcast{value->is<BitCast>()}) {
            return self(self, bitcast->get_value());
        }
        if (const auto load{value->is<Load>()}) {
            return self(self, load->get_addr());
        }
        return false;
    };

    const auto stack_access_in_inst = [&](const std::shared_ptr<Instruction> &inst) -> bool {
        switch (inst->get_op()) {
            case Operator::LOAD: {
                return access_stack(access_stack, inst->as<Load>()->get_addr());
            }
            case Operator::STORE: {
                return access_stack(access_stack, inst->as<Store>()->get_addr());
            }
            case Operator::CALL: {
                const auto _call{inst->as<Call>()};
                const auto params{_call->get_params()};
                return std::any_of(params.begin(), params.end(),
                                   [&](const auto &param) { return access_stack(access_stack, param); });
            }
            default:
                break;
        }
        return false;
    };

    const auto stack_access_in_block = [&](const std::shared_ptr<Block> &block) -> bool {
        const auto &instructions{block->get_instructions()};
        return std::any_of(instructions.begin(), instructions.end(), stack_access_in_inst);
    };

    // 如果找到一条带有栈访问的路径，则返回 true
    // 如果从 current 到 end_block 的所有路径都安全，则返回 false
    const auto has_stack_access_on_path = [&](auto &&self, const std::shared_ptr<Block> &current) -> bool {
        // 环路检测：如果我们已经访问过此节点，可以认为这个环是安全的
        // （否则在第一次进入环时就应该已经检测到栈访问了）
        if (visited.count(current)) {
            return false;
        }
        visited.insert(current);
        // 当前块包含栈访问，我们已经找到了一条不安全的路径
        if (stack_access_in_block(current)) {
            visited.erase(current); // 为了其他路径的探索，回溯
            return true;
        }
        // 递归步骤：检查所有后继块，只要【任何一个】后继块导向不安全路径，那么当前路径就是不安全的
        if (current == end_block) {
            return false;
        }
        const auto &successors{cfg.successors.at(current)};
        for (const auto &succ: successors) {
            if (self(self, succ)) {
                visited.erase(current);
                return true; // 向上层传播“不安全”信号 (true)
            }
        }
        visited.erase(current);
        // 如果从这里出发的所有路径都是安全的，那么此子路径是安全的
        return false;
    };

    // 1. 首先检查在【同一个块内】，`call` 指令之后的指令是否有栈访问
    const auto call_iter{Pass::Utils::inst_as_iter(call)};
    if (!call_iter) [[unlikely]] {
        log_error("Call should in block");
        return false; // 如果调用不在块内，无法优化
    }
    if (std::any_of(std::next(call_iter.value()), start_block->get_instructions().end(), stack_access_in_inst)) {
        return false; // 如果在同一块的后续指令中有访问，则不安全
    }
    // 2. 检查从当前块的【后继块】开始的所有路径。
    // 使用 `std::any_of`，因为只要【任何一条】后续路径不安全，优化就失败了
    // `has_stack_access_on_path` 在路径不安全时返回 true
    if (const auto &succs{cfg.successors.at(start_block)};
        std::any_of(succs.begin(), succs.end(), [&](const auto &succ) {
            // visited 集合的管理通过递归中的 insert/erase 来完成，
            return has_stack_access_on_path(has_stack_access_on_path, succ);
        })) {
        // 找到了一条不安全的路径
        return false;
    }
    // 如果当前块的后续指令和所有后续路径都安全，则此调用是安全的
    return true;
}
} // namespace

namespace Pass {
void TailCallOptimize::tail_call_detect(const std::shared_ptr<Function> &func) const {
    std::vector<std::shared_ptr<Call>> candidates;
    for (const auto &block: func->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() != Operator::CALL) [[likely]] {
                continue;
            }
            const auto &call{inst->as<Call>()};
            if (call->get_function()->as<Function>()->is_runtime_func()) {
                continue;
            }
            candidates.push_back(call);
        }
    }
    // 收集当前函数中所有的 alloca 指令（栈帧内存）
    std::unordered_set<std::shared_ptr<Value>> stack_allocs;
    for (const auto &block: func->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() == Operator::ALLOC) {
                stack_allocs.insert(inst);
            }
        }
    }

    // 从 call 到 ret 的所有路径上，不能存在对调用者函数栈帧上由 alloca
    // 指令分配的内存的读写操作 一旦栈帧被重用，这些内存就会失效
    const auto is_valid_call = [&](const std::shared_ptr<Call> &call) -> bool {
        // log_info("%s", call->to_string().c_str());
        // 检查从 call 指令所在块到所有 ret 指令的路径
        const auto call_block = call->get_block();
        const auto &cfg = cfg_info->graph(func);
        // 收集所有包含 ret 指令的块
        std::vector<std::shared_ptr<Block>> ret_blocks;
        for (const auto &block: func->get_blocks()) {
            if (const auto last_inst = block->get_instructions().back(); last_inst->get_op() == Operator::RET) {
                ret_blocks.push_back(block);
            }
        }
        // 检查从 call 块到每个 ret 块的路径
        return std::all_of(ret_blocks.begin(), ret_blocks.end(), [&](const auto &ret_block) {
            return path_without_stack_access(call, ret_block, stack_allocs, cfg);
        });
    };

    // 过滤出可以进行尾调用优化的候选调用
    std::vector<std::shared_ptr<Call>> valid_calls;
    for (const auto &call: candidates) {
        if (is_valid_call(call)) {
            valid_calls.push_back(call);
        }
    }
    // 将 call 指令标记为 tail call 的逻辑
    std::for_each(valid_calls.begin(), valid_calls.end(), [](const auto &call) { call->set_tail_call(); });
}

// 参见：https://github.com/llvm/llvm-project/blob/main/llvm/lib/Transforms/Scalar/TailRecursionElimination.cpp
void TailCallOptimize::tail_call_eliminate(const std::shared_ptr<Function> &func) const {
    const auto &func_data{func_info->func_info(func)};
    if (!func_data.is_recursive) {
        return;
    }
    if (func_data.memory_alloc || func_data.has_side_effect || func_data.memory_write || !func_data.no_state) {
        return;
    }
    // 第一个基本块不应该存在尾递归调用
    if (const auto &entry{func->get_blocks().front()}; entry->get_instructions().empty()) {
        return;
    }
}

void TailCallOptimize::run_on_func(const std::shared_ptr<Function> &func) const {
    tail_call_eliminate(func);
    tail_call_detect(func);
}

void TailCallOptimize::transform(const std::shared_ptr<Module> module) {
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    func_info = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    cfg_info = nullptr;
    func_info = nullptr;
}
} // namespace Pass

#include <unordered_set>

#include "Pass/Util.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
bool path_without_stack_access(const std::shared_ptr<Call> &call, const std::shared_ptr<Block> &end_block,
                               const std::unordered_set<std::shared_ptr<Value>> &stack_allocs,
                               const Pass::ControlFlowGraph::Graph &cfg) {
    const auto start_block{call->get_block()};
    std::unordered_set<std::shared_ptr<Block>> visited;

    const auto access_stack = [&stack_allocs](auto &&self, const std::shared_ptr<Value> &value) -> bool {
        // 直接检查是否是栈分配的内存
        if (stack_allocs.count(value)) {
            return true;
        }
        // 检查通过 getelementptr 访问的栈内存
        if (const auto gep{value->is<GetElementPtr>()}) {
            return self(self, gep->get_addr());
        }
        // 检查通过 BitCast 访问的栈内存
        if (const auto bitcast{value->is<BitCast>()}) {
            return self(self, bitcast->get_value());
        }
        // 检查通过 Load 访问的栈内存
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
                return std::any_of(params.begin(), params.end(), [&](const auto &param) {
                    return access_stack(access_stack, param);
                });
            }
            default: break;
        }
        return false;
    };

    const auto stack_access_in_block = [&](const std::shared_ptr<Block> &block) -> bool {
        const auto &instructions{block->get_instructions()};
        return std::any_of(instructions.begin(), instructions.end(), stack_access_in_inst);
    };

    // dfs 查找从 start_block 到 end_block 的路径
    const auto dfs = [&](auto &&self, const std::shared_ptr<Block> &current) -> bool {
        if (visited.count(current))
            return false;
        visited.insert(current);
        // 检查当前块是否包含对栈内存的访问
        if (stack_access_in_block(current)) {
            visited.erase(current);
            return false;
        }
        const auto &successors{cfg.successors.at(current)};
        for (const auto &succ: successors) {
            if (self(self, succ)) {
                return true;
            }
        }
        visited.erase(current);
        return false;
    };

    const auto call_iter{Pass::Utils::inst_as_iter(call)};
    if (!call_iter) [[unlikely]] {
        log_error("Call should in block");
    }

    if (std::any_of(std::next(call_iter.value()), start_block->get_instructions().end(), stack_access_in_inst)) {
        return false;
    }

    const auto &succs{cfg.successors.at(start_block)};
    return std::none_of(succs.begin(), succs.end(), [&](const auto &succ) { return dfs(dfs, succ); });
}
}

namespace Pass {
void TailCallOptimize::run_on_func(const std::shared_ptr<Function> &func) const {
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
            if (const auto last_inst = block->get_instructions().back();
                last_inst->get_op() == Operator::RET) {
                ret_blocks.push_back(block);
            }
        }
        // 检查从 call 块到每个 ret 块的路径
        if (std::any_of(ret_blocks.begin(), ret_blocks.end(), [&](const auto &ret_block) {
            return !path_without_stack_access(call, ret_block, stack_allocs, cfg);
        })) {
            return false;
        }

        // 检查返回值处理：如果 call 有返回值，那么这个返回值只能被用于：
        // a. 直接作为调用者函数的返回值（例如 call 后面紧跟 ret）。
        // b.
        // 通过一系列计算（如算术运算、类型转换等），但这些计算的最终结果必须是调用者函数的返回值，
        //    并且计算过程中不能依赖栈上的局部变量。
        // c. 通过 phi 节点传递，并最终成为函数的返回值。
        if (!call->get_type()->is_void()) {
            // TODO
        }

        return true;
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

void TailCallOptimize::transform(const std::shared_ptr<Module> module) {
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    func_info = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    cfg_info = nullptr;
    func_info = nullptr;
}
}

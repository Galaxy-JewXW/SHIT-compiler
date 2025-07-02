#include <unordered_set>

#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
// 检查值是否访问栈内存
bool accesses_stack_memory(const std::shared_ptr<Value> &value,
                           const std::unordered_set<std::shared_ptr<Value>> &stack_allocs) {
    // 直接检查是否是栈分配的内存
    if (stack_allocs.count(value)) {
        return true;
    }
    // 检查通过 getelementptr 访问的栈内存
    if (const auto gep = std::dynamic_pointer_cast<GetElementPtr>(value)) {
        return accesses_stack_memory(gep->get_addr(), stack_allocs);
    }
    // 检查通过 BitCast 访问的栈内存
    if (const auto bitcast = std::dynamic_pointer_cast<BitCast>(value)) {
        return accesses_stack_memory(bitcast->get_value(), stack_allocs);
    }
    // 检查通过 Load 访问的栈内存
    if (const auto load = std::dynamic_pointer_cast<Load>(value)) {
        return accesses_stack_memory(load->get_addr(), stack_allocs);
    }
    return false;
}

// 检查基本块是否包含对栈内存的访问
bool block_has_stack_access(const std::shared_ptr<Block> &block,
                            const std::unordered_set<std::shared_ptr<Value>> &stack_allocs) {
    for (const auto &inst: block->get_instructions()) {
        // 检查 load 指令
        if (inst->get_op() == Operator::LOAD) {
            if (const auto load = inst->as<Load>();
                accesses_stack_memory(load->get_addr(), stack_allocs)) {
                return true;
            }
        } else if (inst->get_op() == Operator::STORE) {
            // 检查 store 指令
            if (const auto store = inst->as<Store>();
                accesses_stack_memory(store->get_addr(), stack_allocs)) {
                return true;
            }
        } else if (inst->get_op() == Operator::CALL) {
            // 检查 call 指令（可能通过参数传递栈内存地址）
            const auto call = inst->as<Call>();
            for (const auto &param: call->get_params()) {
                if (accesses_stack_memory(param, stack_allocs)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// 检查从 start_block 到 end_block 是否存在不访问栈内存的路径
bool path_without_stack_access(const std::shared_ptr<Block> &start_block,
                               const std::shared_ptr<Block> &end_block,
                               const std::unordered_set<std::shared_ptr<Value>> &stack_allocs,
                               const Pass::ControlFlowGraph::Graph &cfg) {
    std::unordered_set<std::shared_ptr<Block>> visited;
    std::unordered_set<std::shared_ptr<Block>> path_blocks;

    // DFS 查找从 start_block 到 end_block 的路径
    auto dfs = [&](auto &&self, const std::shared_ptr<Block> &current) -> bool {
        if (current == end_block) {
            return true;
        }

        if (visited.count(current)) {
            return false;
        }

        visited.insert(current);
        path_blocks.insert(current);

        // 检查当前块是否包含对栈内存的访问
        if (block_has_stack_access(current, stack_allocs)) {
            path_blocks.erase(current);
            visited.erase(current);
            return false;
        }

        // 继续搜索后继块
        const auto &successors = cfg.successors.at(current);
        for (const auto &succ: successors) {
            if (self(self, succ)) {
                return true;
            }
        }

        path_blocks.erase(current);
        visited.erase(current);
        return false;
    };

    return dfs(dfs, start_block);
}

bool is_valid_return_value_usage(const std::shared_ptr<Instruction> &user, const Call *call,
                                 const std::unordered_set<std::shared_ptr<Value>> &stack_allocs);

bool is_phi_ultimately_returned(const std::shared_ptr<Instruction> &phi,
                                const std::shared_ptr<Function> &func,
                                const std::unordered_set<std::shared_ptr<Value>> &stack_allocs);

bool is_allowed_computation(const std::shared_ptr<Instruction> &inst);

bool is_computation_ultimately_returned(const std::shared_ptr<Instruction> &inst,
                                        const std::shared_ptr<Function> &func,
                                        const std::unordered_set<std::shared_ptr<Value>> &stack_allocs);

bool check_ultimately_returned(const std::shared_ptr<Instruction> &inst,
                               const std::shared_ptr<Function> &func,
                               const std::unordered_set<std::shared_ptr<Value>> &stack_allocs,
                               std::unordered_set<std::shared_ptr<Instruction>> &visited);

// 检查 call 的返回值使用是否有效
bool is_return_value_usage_valid(const Call *call,
                                 const std::unordered_set<std::shared_ptr<Value>> &stack_allocs) {
    // 收集所有使用 call 返回值的指令
    std::vector<std::shared_ptr<Instruction>> users;

    // 获取 call 指令的所有用户（需要 const_cast 因为 users() 不是 const）
    for (const auto &user: const_cast<Call *>(call)->users()) {
        if (const auto inst = std::dynamic_pointer_cast<Instruction>(user)) {
            users.push_back(inst);
        }
    }

    // 如果没有用户，说明返回值未被使用，这是允许的
    if (users.empty())
        return true;

    // 检查每个用户是否满足尾调用优化的要求
    return std::all_of(users.begin(), users.end(), [&](const std::shared_ptr<Instruction> &user) {
        return is_valid_return_value_usage(user, call, stack_allocs);
    });
}

// 检查单个用户是否满足尾调用优化的要求
bool is_valid_return_value_usage(const std::shared_ptr<Instruction> &user, const Call *call,
                                 const std::unordered_set<std::shared_ptr<Value>> &stack_allocs) {
    // 检查是否是 ret 指令（直接作为函数返回值）
    if (user->get_op() == Operator::RET) {
        return true;
    }
    // 检查是否是 phi 节点
    if (user->get_op() == Operator::PHI) {
        return is_phi_ultimately_returned(user, call->get_block()->get_function(),
                                          stack_allocs);
    }
    // 检查是否是允许的计算指令（算术运算、类型转换等）
    if (is_allowed_computation(user)) {
        return is_computation_ultimately_returned(user, call->get_block()->get_function(), stack_allocs);
    }
    // 其他情况都不允许
    return false;
}

// 检查是否是允许的计算指令
bool is_allowed_computation(const std::shared_ptr<Instruction> &inst) {
    switch (inst->get_op()) {
        case Operator::INTBINARY:
        case Operator::FLOATBINARY:
        case Operator::FLOATTERNARY:
        case Operator::ICMP:
        case Operator::FCMP:
        case Operator::ZEXT:
        case Operator::FPTOSI:
        case Operator::SITOFP:
        case Operator::BITCAST:
        case Operator::FNEG:
            return true;
        default:
            return false;
    }
}

// 检查 phi 节点是否最终被作为返回值
bool is_phi_ultimately_returned(const std::shared_ptr<Instruction> &phi,
                                const std::shared_ptr<Function> &func,
                                const std::unordered_set<std::shared_ptr<Value>> &stack_allocs) {
    std::unordered_set<std::shared_ptr<Instruction>> visited;
    return check_ultimately_returned(phi, func, stack_allocs, visited);
}

// 检查计算指令是否最终被作为返回值
bool is_computation_ultimately_returned(const std::shared_ptr<Instruction> &inst,
                                        const std::shared_ptr<Function> &func,
                                        const std::unordered_set<std::shared_ptr<Value>> &stack_allocs) {
    std::unordered_set<std::shared_ptr<Instruction>> visited;
    return check_ultimately_returned(inst, func, stack_allocs, visited);
}

// 递归检查指令是否最终被作为返回值
bool check_ultimately_returned(const std::shared_ptr<Instruction> &inst,
                               const std::shared_ptr<Function> &func,
                               const std::unordered_set<std::shared_ptr<Value>> &stack_allocs,
                               std::unordered_set<std::shared_ptr<Instruction>> &visited) {
    // 避免循环依赖
    if (visited.count(inst)) {
        return false;
    }
    visited.insert(inst);
    // 检查当前指令的所有用户（需要 const_cast 因为 users() 不是 const）
    for (const auto &user: const_cast<Instruction *>(inst.get())->users()) {
        if (const auto user_inst = std::dynamic_pointer_cast<Instruction>(user)) {
            // 如果用户是 ret 指令，说明最终被作为返回值
            if (user_inst->get_op() == Operator::RET) {
                return true;
            }
            // 如果用户是 phi 节点，递归检查
            if (user_inst->get_op() == Operator::PHI) {
                if (check_ultimately_returned(user_inst, func, stack_allocs, visited)) {
                    return true;
                }
            }
            // 如果用户是允许的计算指令，递归检查
            if (is_allowed_computation(user_inst)) {
                if (check_ultimately_returned(user_inst, func, stack_allocs, visited)) {
                    return true;
                }
            }
            // 其他情况都不允许
            return false;
        }
    }
    // 如果没有用户，说明没有被使用，这是不允许的
    return false;
}
} // namespace

namespace Pass {
void TCO::run_on_func(const std::shared_ptr<Function> &func) const {
    std::vector<const Call *> candidates;
    for (const auto &block: func->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() != Operator::CALL) [[likely]] {
                continue;
            }
            const auto &call{inst->as<Call>()};
            if (call->get_function()->as<Function>()->is_runtime_func()) {
                continue;
            }
            candidates.push_back(call.get());
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
    const auto is_valid_call = [&](const Call *call) -> bool {
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
        for (const auto &ret_block: ret_blocks) {
            if (!path_without_stack_access(call_block, ret_block, stack_allocs,
                                           cfg)) {
                return false;
            }
        }

        // 检查返回值处理：如果 call 有返回值，那么这个返回值只能被用于：
        // a. 直接作为调用者函数的返回值（例如 call 后面紧跟 ret）。
        // b.
        // 通过一系列计算（如算术运算、类型转换等），但这些计算的最终结果必须是调用者函数的返回值，
        // 并且计算过程中不能依赖栈上的局部变量。
        // c. 通过 phi 节点传递，并最终成为函数的返回值。
        if (!call->get_type()->is_void()) {
            if (!is_return_value_usage_valid(call, stack_allocs)) {
                return false;
            }
        }

        return true;
    };
    // 过滤出可以进行尾调用优化的候选调用
    std::vector<const Call *> valid_calls;
    for (const auto &call: candidates) {
        if (is_valid_call(call)) {
            valid_calls.push_back(call);
        }
    }
    // TODO: 对有效的尾调用进行优化标记
    // 这里可以添加将 call 指令标记为 tail call 的逻辑
}

void TCO::transform(const std::shared_ptr<Module> module) {
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    func_info = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    cfg_info = nullptr;
    func_info = nullptr;
}
} // namespace Pass

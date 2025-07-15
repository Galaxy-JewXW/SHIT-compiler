#include <memory>
#include <optional>
#include <unordered_set>

#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Util.h"

using namespace Mir;

namespace {
std::shared_ptr<Call> find_tre_candidate(const std::shared_ptr<Block> &block) {
    const auto &func{block->get_function()};
    for (auto it{block->get_instructions().rbegin()}; it != block->get_instructions().rend(); ++it) {
        // log_info("%s", (*it)->to_string().c_str());
        if ((*it)->get_op() != Operator::CALL) [[likely]] {
            continue;
        }
        const auto call{(*it)->as<Call>()};
        if (call->get_function() != func || !call->is_tail_call()) {
            continue;
        }
        return call;
    }
    return nullptr;
}

// 检查从 call 到 end_block 的所有路径是否都不含栈访问
// 如果安全，返回 true；否则返回 false
[[maybe_unused]]
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

// 获取二元运算的恒等元素（单位元）
// 恒等元用于在 phi 节点初始化时能够正确地表示“还没累积任何值”的状态
std::shared_ptr<Value> get_identity_element(const std::shared_ptr<Instruction> &inst) {
    const auto &type{inst->get_type()};
    switch (inst->as<IntBinary>()->intbinary_op()) {
        case IntBinary::Op::ADD:
        case IntBinary::Op::OR:
        case IntBinary::Op::XOR:
            return ConstInt::create(0, type);
        case IntBinary::Op::AND:
            return ConstInt::create(-1, type);
        case IntBinary::Op::MUL:
            return ConstInt::create(1, type);
        default:
            log_error("Invalid instruction type %s", inst->to_string().c_str());
    }
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
    std::copy_if(candidates.begin(), candidates.end(), std::back_inserter(valid_calls), is_valid_call);
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
    if (const auto &entry{func->get_blocks().front()}; entry->get_instructions().empty() || find_tre_candidate(entry)) {
        return;
    }

    const auto handle_block = [&](const std::shared_ptr<Block> &block) -> bool {
        const auto &terminator{block->get_instructions().back()};
        if (const auto type{terminator->get_op()}; type == Operator::RET) {
            if (const auto call{find_tre_candidate(block)}) {
                return handle_tail_call(call);
            }
        } else if (type == Operator::JUMP) {
            const auto &target_block{terminator->as<Jump>()->get_target_block()};
            for (const auto &inst: target_block->get_instructions()) {
                if (const auto _type{inst->get_op()}; _type == Operator::PHI) {
                    continue;
                } else if (_type != Operator::RET) {
                    break;
                }
                const auto ret{inst->as<Ret>()};
                const auto call{find_tre_candidate(block)};
                if (call == nullptr) {
                    break;
                }
                block->get_instructions().pop_back();
                const auto new_ret =
                        func->get_return_type()->is_void() ? Ret::create(block) : Ret::create(ret->get_value(), block);
                if (!func->get_return_type()->is_void()) {
                    const auto returned_value{new_ret->get_value()};
                    if (const auto phi{returned_value->is<Phi>()}; phi && phi->get_block() == target_block) {
                        new_ret->modify_operand(returned_value, phi->get_optional_values()[block]);
                    } else {
                        // 撤回修改操作
                        block->get_instructions().pop_back();
                        block->get_instructions().push_back(ret);
                        return false;
                    }
                }
                for (const auto &phi: target_block->get_instructions()) {
                    if (phi->get_op() != Operator::PHI) {
                        break;
                    }
                    phi->as<Phi>()->remove_optional_value(block);
                }
                handle_tail_call(call);
                return true;
            }
            return false;
        }
        return false;
    };

    for (const auto &block: func->get_blocks()) {
        if (handle_block(block)) {
            set_analysis_result_dirty<ControlFlowGraph>(func);
            return;
        }
    }
}

bool TailCallOptimize::handle_tail_call(const std::shared_ptr<Call> &call) {
    const auto block{call->get_block()};
    const auto func{block->get_function()};

    // 基本块的终结指令
    const auto ret{block->get_instructions().back()};
    // 获取调用指令的迭代器
    const auto call_it{Utils::inst_as_iter(call)};
    if (!call_it.has_value()) [[unlikely]] {
        log_error("Instruction %s not in block %s", call->to_string().c_str(), block->get_name().c_str());
    }

    // 获取调用指令的下一条指令
    const std::shared_ptr next_inst{*std::next(call_it.value())};
    // 累加器指令（如果存在）
    std::shared_ptr<IntBinary> accumulator{nullptr};

    // 检查调用后是否有累加操作（return f() + acc）
    if (const auto op = next_inst->get_op(); op == Operator::INTBINARY) {
        const auto intbinary{next_inst->as<IntBinary>()};
        // 只处理满足交换律和结合律的运算
        if (!intbinary->is_commutative() || !intbinary->is_associative()) {
            return false;
        }
        accumulator = intbinary;
        // 确保递归调用结果只被这个累加器使用一次
        if (std::count(accumulator->get_operands().begin(), accumulator->get_operands().end(), call) != 1) {
            return false;
        }
        // 确保累加器的结果只被return语句使用
        if (!(accumulator->users().size() == 1 && *accumulator->users().begin() == ret)) {
            return false;
        }
    } else if (op != Operator::RET) {
        // 调用后如果不是累加操作也不是return，则无法优化
        return false;
    }

    // 创建新的入口基本块，用于循环结构
    // 原入口块
    const auto old_entry{func->get_blocks().front()};
    // 新入口块
    const auto new_entry{Block::create("new_entry")};
    new_entry->set_function(func, false);
    func->get_blocks().insert(func->get_blocks().begin(), new_entry);
    // 从新入口跳转到原入口
    Jump::create(old_entry, new_entry);

    // 为每个函数参数创建phi节点，用于在循环中更新参数值
    for (size_t i{0}; i < func->get_arguments().size(); ++i) {
        const auto &arg{func->get_arguments()[i]};

        // 创建phi节点来合并来自不同路径的参数值
        const auto phi{Phi::create("phi", arg->get_type(), nullptr, {})};
        phi->set_block(old_entry, false);
        old_entry->get_instructions().insert(old_entry->get_instructions().begin(), phi);

        // 用phi节点替换原来的参数
        arg->replace_by_new_value(phi);

        // 设置phi节点的两个来源：
        // 1. 首次进入：使用原参数值
        // 2. 循环回来：使用递归调用的参数
        phi->set_optional_value(new_entry, arg);
        phi->set_optional_value(block, call->get_params()[i]);
    }

    // 为返回值和累加器创建相应的phi节点
    std::shared_ptr<Phi> ret_value{nullptr}; // 返回值的phi节点
    std::shared_ptr<Phi> ret_valid{nullptr}; // 标记返回值是否有效的phi节点
    std::shared_ptr<Phi> acc_value{nullptr}; // 累加器值的phi节点

    // 如果函数有返回值，创建返回值相关的phi节点
    if (!call->get_type()->is_void()) {
        ret_value = Phi::create("ret_value", call->get_type(), nullptr, {});
        ret_value->set_block(old_entry, false);
        old_entry->get_instructions().insert(old_entry->get_instructions().begin(), ret_value);
        ret_value->set_optional_value(new_entry, Undef::create(call->get_type())); // 初始值未定义

        ret_valid = Phi::create("ret_valid", Type::Integer::i1, nullptr, {});
        ret_valid->set_block(old_entry, false);
        old_entry->get_instructions().insert(old_entry->get_instructions().begin(), ret_valid);
        ret_valid->set_optional_value(new_entry, ConstBool::create(0)); // 初始时返回值无效
    }
    // 如果存在累加器，创建累加器的phi节点
    if (accumulator) {
        acc_value = Phi::create("acc_value", accumulator->get_type(), nullptr, {});
        acc_value->set_block(old_entry, false);
        old_entry->get_instructions().insert(old_entry->get_instructions().begin(), acc_value);
        acc_value->set_optional_value(new_entry, get_identity_element(accumulator)); // 使用恒等元初始化

        // 用累加器的phi节点替换递归调用
        call->replace_by_new_value(acc_value);

        if (call->users().size() != 0) {
            log_error("Shouldn't reach here");
        }
    }
    // 用于存储条件选择指令的向量
    std::vector<std::shared_ptr<Select>> selects;
    // 处理返回值的phi节点设置
    if (ret_value) {
        if (acc_value || call->users().size() > 0) {
            // 如果有累加器或调用有其他用户，直接设置phi值
            ret_value->set_optional_value(block, ret_value);
            ret_valid->set_optional_value(block, ret_valid);
        } else {
            // 创建条件选择：如果已有有效返回值则使用它，否则使用当前返回值
            const auto select{Select::create("select", ret_valid, ret_value, ret->get_operands()[0], block)};
            selects.push_back(select);
            Utils::move_instruction_before(select, ret);
            ret_value->set_optional_value(block, select);
            // 标记返回值现在有效
            ret_valid->set_optional_value(block, ConstBool::create(1));
        }

        if (acc_value) {
            // 设置累加器的循环值
            acc_value->set_optional_value(block, accumulator);
        }
    }
    // 修改控制流：移除return，添加跳转回循环头部，移除递归调用
    block->get_instructions().pop_back();
    const auto jump{Jump::create(old_entry, block)};
    block->get_instructions().erase(Utils::inst_as_iter(call).value());
    // 处理函数中所有的return语句，确保正确处理返回值和累加
    if (ret_value) {
        if (selects.empty()) {
            // 如果没有条件选择，移除不需要的phi节点
            old_entry->get_instructions().erase(Utils::inst_as_iter(ret_value).value());
            old_entry->get_instructions().erase(Utils::inst_as_iter(ret_valid).value());
            // 为每个return语句添加累加操作
            if (acc_value && accumulator) {
                for (const auto &b: func->get_blocks()) {
                    if (const auto terminator{b->get_instructions().back()}; terminator->get_op() == Operator::RET) {
                        const auto _ret_value{terminator->as<Ret>()->get_value()};
                        // 克隆累加器指令
                        const auto _acc{accumulator->clone()};
                        // 修改累加器操作数，将累加器值替换为当前返回值
                        _acc->modify_operand(_acc->get_operands()[_acc->get_operands()[0] == acc_value], _ret_value);
                        Utils::move_instruction_before(_acc, terminator);
                        terminator->as<Ret>()->modify_operand(_ret_value, _acc);
                    }
                }
            }
        } else {
            // 为每个return语句创建条件选择
            for (const auto &b: func->get_blocks()) {
                if (const auto terminator{b->get_instructions().back()}; terminator->get_op() == Operator::RET) {
                    const auto _ret_value{terminator->as<Ret>()->get_value()};
                    const auto select{Select::create("select", ret_valid, ret_value, _ret_value, block)};
                    Utils::move_instruction_before(select, terminator);
                    selects.push_back(select);
                    terminator->as<Ret>()->modify_operand(_ret_value, select);
                }
            }
            // 为每个条件选择添加累加操作
            if (acc_value) {
                for (const auto &select: selects) {
                    const auto _val{select->get_false_value()};
                    const auto _acc{accumulator->clone()};
                    _acc->modify_operand(_acc->get_operands()[_acc->get_operands()[0] == acc_value], _val);
                    Utils::move_instruction_before(_acc, select);
                    select->modify_operand(_val, _acc);
                }
            }
        }
    }
    return true;
}

void TailCallOptimize::run_on_func(const std::shared_ptr<Function> &func) const {
    tail_call_detect(func);
    tail_call_eliminate(func);
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

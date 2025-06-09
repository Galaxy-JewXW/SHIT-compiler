#include <optional>

#include "Pass/Util.h"
#include "Pass/Analyses/FunctionAnalysis.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
bool is_recursive_call(const std::shared_ptr<Function> &func, const std::shared_ptr<Call> &call) {
    return call->get_function() == func;
}

// 从基本块的指令列表末尾开始向前遍历，寻找最后一个递归调用
std::optional<std::shared_ptr<Call>> get_last_recursive_call(const std::shared_ptr<Function> &func,
                                                             const std::shared_ptr<Block> &block) {
    for (auto it{block->get_instructions().rbegin()}; it != block->get_instructions().rend(); ++it) {
        if ((*it)->get_op() != Operator::CALL) {
            continue;
        }
        if (const auto &call{(*it)->as<Call>()}; is_recursive_call(func, call)) {
            return std::make_optional(call);
        }
    }
    return std::nullopt;
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

bool eliminate_call(const std::shared_ptr<Call> &call, const std::shared_ptr<Function> &func) {
    // 包含调用的基本块
    const auto block{call->get_block()};
    // 基本块的终结指令
    const auto ret{block->get_instructions().back()};
    // 获取调用指令的迭代器
    const auto call_it{Pass::Utils::inst_as_iter(call)};
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
        if (const auto count = static_cast<size_t>(std::count(accumulator->get_operands().begin(),
                                                              accumulator->get_operands().end(), call));
            count != 1) {
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
        // 首次进入：使用原参数值
        // 循环回来：使用递归调用的参数
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
            Pass::Utils::move_instruction_before(select, ret);
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
    block->get_instructions().erase(Pass::Utils::inst_as_iter(call).value());
    // 处理函数中所有的return语句，确保正确处理返回值和累加
    if (ret_value) {
        if (selects.empty()) {
            // 如果没有条件选择，移除不需要的phi节点
            old_entry->get_instructions().erase(Pass::Utils::inst_as_iter(ret_value).value());
            old_entry->get_instructions().erase(Pass::Utils::inst_as_iter(ret_valid).value());
            // 为每个return语句添加累加操作
            if (acc_value && accumulator) {
                for (const auto &b: func->get_blocks()) {
                    if (const auto terminator{b->get_instructions().back()};
                        terminator->get_op() == Operator::RET) {
                        const auto _ret_value{terminator->as<Ret>()->get_value()};
                        // 克隆累加器指令
                        const auto _acc{accumulator->clone()};
                        // 修改累加器操作数，将累加器值替换为当前返回值
                        _acc->modify_operand(_acc->get_operands()[_acc->get_operands()[0] == acc_value], _ret_value);
                        Pass::Utils::move_instruction_before(_acc, terminator);
                        terminator->as<Ret>()->modify_operand(_ret_value, _acc);
                    }
                }
            }
        } else {
            // 为每个return语句创建条件选择
            for (const auto &b: func->get_blocks()) {
                if (const auto terminator{b->get_instructions().back()};
                    terminator->get_op() == Operator::RET) {
                    const auto _ret_value{terminator->as<Ret>()->get_value()};
                    const auto select{Select::create("select", ret_valid, ret_value, _ret_value, block)};
                    Pass::Utils::move_instruction_before(select, terminator);
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
                    Pass::Utils::move_instruction_before(_acc, select);
                    select->modify_operand(_val, _acc);
                }
            }
        }
    }
    return true;
}
}

namespace Pass {
void TailRecursionToLoop::run_on_func(const std::shared_ptr<Function> &func) const {
    const auto &func_data{func_info->func_info(func)};
    std::unordered_set<std::shared_ptr<Instruction>> deleted_instructions;
    if (!func_data.is_recursive) {
        return;
    }
    if (func_data.memory_alloc || func_data.has_side_effect || func_data.memory_write || !func_data.no_state) {
        return;
    }
    // 第一个基本块不应该存在尾递归调用
    if (const auto &entry{func->get_blocks().front()};
        entry->get_instructions().empty() || get_last_recursive_call(func, entry)) {
        return;
    }
    const auto handle = [&](const std::shared_ptr<Block> &block) -> bool {
        const auto terminator{block->get_instructions().back()}; // 基本块的终结指令

        if (const auto inst_type{terminator->get_op()}; inst_type == Operator::RET) {
            // 基本块以return结尾
            if (const auto recursive_call{get_last_recursive_call(func, block)};
                recursive_call.has_value()) {
                return eliminate_call(recursive_call.value(), func);
            }
        } else if (inst_type == Operator::BRANCH) {
            // 基本块以条件分支结尾
            const auto target_block{terminator->as<Branch>()->get_true_block()};
            // 检查目标块是否只包含phi节点和return语句
            for (const auto &inst: target_block->get_instructions()) {
                if (const auto type{inst->get_op()}; type == Operator::PHI) {
                    continue; // 跳过phi节点
                } else if (type != Operator::RET) {
                    break; // 如果不是return，停止检查
                }
                const auto ret{inst->as<Ret>()};
                const auto recursive_call{get_last_recursive_call(func, block)};
                if (!recursive_call.has_value()) {
                    return false;
                }
                // 修改控制流：将分支改为直接返回
                block->get_instructions().pop_back(); // 移除分支指令
                // 创建新的return指令
                const auto new_ret = ret->get_operands().empty()
                                         ? Ret::create(block)
                                         : Ret::create(ret->get_value(), block);
                // 处理返回值（如果存在）
                if (!new_ret->get_operands().empty()) {
                    const auto returned_value = new_ret->get_value();
                    // 如果返回值是目标块的phi节点，需要获取对应的值
                    if (const auto returned_phi = returned_value->is<Phi>();
                        returned_phi != nullptr && returned_phi->get_block() == target_block) {
                        ret->modify_operand(returned_value, returned_phi->get_optional_values()[block]);
                    } else {
                        // 无法处理，回滚更改
                        block->get_instructions().pop_back();
                        block->get_instructions().push_back(terminator);
                        return false;
                    }
                }
                // 清理目标块中的phi节点
                for (const auto &phi: target_block->get_instructions()) {
                    if (phi->get_op() != Operator::PHI) {
                        break;
                    }
                    phi->as<Phi>()->remove_optional_value(block);
                }
                // 消除递归调用
                eliminate_call(recursive_call.value(), func);
                return true;
            }
        }
        return false;
    };

    for (const auto &block: func->get_blocks()) {
        if (handle(block)) {
            set_analysis_result_dirty<ControlFlowGraph>(func);
            return;
        }
    }
}

void TailRecursionToLoop::transform(const std::shared_ptr<Module> module) {
    func_info = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    func_info = nullptr;
}
}

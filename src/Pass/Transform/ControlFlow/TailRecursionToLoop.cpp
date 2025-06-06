#include <optional>

#include "Pass/Util.h"
#include "Pass/Analyses/FunctionAnalysis.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
bool is_recursive_call(const std::shared_ptr<Function> &func, const std::shared_ptr<Call> &call) {
    return call->get_function() == func;
}

std::shared_ptr<Value> gen_zero(const std::shared_ptr<Type::Type> &type) {
    if (type->is_int32()) {
        return ConstInt::create(0);
    }
    if (type->is_int1()) {
        return ConstBool::create(0);
    }
    return ConstFloat::create(0.0);
}

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

bool eliminate_call(const std::shared_ptr<Call> &call, const std::shared_ptr<Block> &block,
                    const std::shared_ptr<Function> &func) {
    const auto ret{block->get_instructions().back()};
    const auto call_it{Pass::Utils::inst_as_iter(call)};
    if (!call_it.has_value()) [[unlikely]] {
        log_error("Instruction %s not in block %s", call->to_string().c_str(), block->get_name().c_str());
    }
    const std::shared_ptr next_inst{*std::next(call_it.value())};
    std::shared_ptr<IntBinary> accumulator{nullptr};
    if (next_inst->get_op() == Operator::INTBINARY) {
        const auto intbinary{next_inst->as<IntBinary>()};
        if (!intbinary->is_commutative() || !intbinary->is_associative()) {
            return false;
        }
        accumulator = intbinary;
        if (const auto count = static_cast<size_t>(std::count(accumulator->get_operands().begin(),
                                                              accumulator->get_operands().end(), call));
            count != 1) {
            return false;
        }
        if (!(accumulator->users().size() == 1 && *accumulator->users().begin() == ret)) {
            return false;
        }
    }

    const auto old_entry{func->get_blocks().front()}, new_entry{Block::create("new_entry")};
    new_entry->set_function(func, false);
    func->get_blocks().insert(func->get_blocks().begin(), new_entry);
    Jump::create(old_entry, new_entry);

    for (size_t i{0}; i < func->get_arguments().size(); ++i) {
        const auto &arg{func->get_arguments()[i]};
        const auto phi{Phi::create("phi", arg->get_type(), nullptr, {})};
        phi->set_block(old_entry, false);
        old_entry->get_instructions().insert(old_entry->get_instructions().begin(), phi);
        arg->replace_by_new_value(phi);
        phi->set_optional_value(new_entry, arg);
        phi->set_optional_value(block, call->get_params()[i]);
    }

    std::shared_ptr<Phi> ret_value{nullptr}, ret_valid{nullptr}, acc_value{nullptr};
    if (!call->get_type()->is_void()) {
        ret_value = Phi::create("ret_value", call->get_type(), nullptr, {});
        ret_value->set_block(old_entry, false);
        old_entry->get_instructions().insert(old_entry->get_instructions().begin(), ret_value);
        ret_value->set_optional_value(new_entry, gen_zero(call->get_type()));

        ret_valid = Phi::create("ret_valid", Type::Integer::i1, nullptr, {});
        ret_valid->set_block(old_entry, false);
        old_entry->get_instructions().insert(old_entry->get_instructions().begin(), ret_valid);
        ret_valid->set_optional_value(new_entry, ConstBool::create(0));
    }
    if (accumulator) {
        acc_value = Phi::create("acc_value", accumulator->get_type(), nullptr, {});
        acc_value->set_block(old_entry, false);
        old_entry->get_instructions().insert(old_entry->get_instructions().begin(), acc_value);
        acc_value->set_optional_value(new_entry, get_identity_element(accumulator));
        call->replace_by_new_value(acc_value);
        if (call->users().size() != 0) {
            log_error("Shouldn't reach here");
        }
    }
    // TODO: select

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
        const auto terminator{block->get_instructions().back()};
        if (const auto inst_type{terminator->get_op()}; inst_type == Operator::RET) {
            if (const auto recursive_call{get_last_recursive_call(func, block)};
                recursive_call.has_value()) {
                return eliminate_call(recursive_call.value(), block, func);
            }
        } else if (inst_type == Operator::BRANCH) {
            const auto target_block{terminator->as<Branch>()->get_true_block()};
            for (const auto &inst: target_block->get_instructions()) {
                if (const auto type{inst->get_op()}; type == Operator::PHI) {
                    continue;
                } else if (type != Operator::RET) {
                    break;
                }
                const auto ret{inst->as<Ret>()};
                const auto recursive_call{get_last_recursive_call(func, block)};
                if (!recursive_call.has_value()) {
                    return false;
                }
                block->get_instructions().pop_back();
                const auto new_ret = [&]() -> std::shared_ptr<Ret> {
                    return ret->get_operands().empty() ? Ret::create(block) : Ret::create(ret->get_value(), block);
                }();
                if (!new_ret->get_operands().empty()) {
                    const auto returned_value = new_ret->get_value();
                    if (const auto returned_phi = returned_value->is<Phi>();
                        returned_phi != nullptr && returned_phi->get_block() == target_block) {
                        ret->modify_operand(returned_value, returned_phi->get_optional_values()[block]);
                    } else {
                        // revert
                        block->get_instructions().pop_back();
                        block->get_instructions().push_back(terminator);
                        return false;
                    }
                }
                for (const auto &phi: target_block->get_instructions()) {
                    if (phi->get_op() != Operator::PHI) {
                        break;
                    }
                    phi->as<Phi>()->remove_optional_value(block);
                }
                eliminate_call(recursive_call.value(), block, func);
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

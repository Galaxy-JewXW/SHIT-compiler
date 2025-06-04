#include <optional>

#include "Pass/Util.h"
#include "Pass/Analyses/FunctionAnalysis.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

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

    auto is_recursive_call = [&](const std::shared_ptr<Call> &call) -> bool {
        return call->get_function() == func;
    };

    auto find_recursive_call = [&](const std::shared_ptr<Block> &block) -> std::optional<std::shared_ptr<Call>> {
        for (auto it{block->get_instructions().rbegin()}; it != block->get_instructions().rend(); ++it) {
            if ((*it)->get_op() != Operator::CALL) {
                continue;
            }
            if (const auto call{(*it)->as<Call>()}; is_recursive_call(call)) {
                return std::make_optional(call);
            }
        }
        return std::nullopt;
    };

    auto find_tail_recursive_call = [&]()-> std::optional<std::shared_ptr<Call>> {
        for (size_t i{1}; i < func->get_blocks().size(); ++i) {
            const auto &block{func->get_blocks()[i]};
            if (block->get_instructions().empty()) {
                continue;
            }
            const auto &terminator = block->get_instructions().back();
            if (terminator->get_op() != Operator::RET) {
                continue;
            }
            const auto ret{terminator->as<Ret>()};
            const auto call{find_recursive_call(block)};
            if (!call.has_value()) {
                continue;
            }
            if (ret->get_operands().empty() || ret->get_value() == call) {
                return std::make_optional(call.value());
            }
        }
        return std::nullopt;
    };

    auto call_to_loop = [&](const std::shared_ptr<Call> &call) -> void {
        const auto parent_block{call->get_block()};
        const auto ret{parent_block->get_instructions().back()->as<Ret>()};
        const auto old_entry{func->get_blocks().front()}, new_entry{Block::create("new_entry")};
        const auto params{call->get_params()};
        Jump::create(old_entry, new_entry);
        func->get_blocks().insert(func->get_blocks().begin(), new_entry);
        for (int i{static_cast<int>(func->get_arguments().size() - 1)}; i >= 0; --i) {
            const auto &arg{func->get_arguments()[i]};
            const auto phi{Phi::create("phi", arg->get_type(), nullptr, {})};
            phi->set_block(old_entry, false);
            old_entry->get_instructions().insert(old_entry->get_instructions().begin(), phi);
            arg->replace_by_new_value(phi);
            phi->set_optional_value(new_entry, arg);
            phi->set_optional_value(parent_block, params[i]);
        }
        deleted_instructions.insert({ret, call});
        Jump::create(old_entry, parent_block);
    };

    const auto old_block_size{func->get_blocks().size()};
    auto target{find_tail_recursive_call()};
    while (target.has_value()) {
        call_to_loop(target.value());
        target = find_tail_recursive_call();
    }
    if (func->get_blocks().size() != old_block_size) {
        get_analysis_result<ControlFlowGraph>(Module::instance())->set_dirty(func);
        func->update_id();
    }

    Utils::delete_instruction_set(Module::instance(), deleted_instructions);
}

void TailRecursionToLoop::transform(const std::shared_ptr<Module> module) {
    func_info = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    func_info = nullptr;
}
}

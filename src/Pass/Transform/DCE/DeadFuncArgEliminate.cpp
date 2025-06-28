#include "Pass/Transforms/DCE.h"

using namespace Mir;

namespace Pass {
void DeadFuncArgEliminate::run_on_func(const std::shared_ptr<Function> &func) const {
    if (func->get_arguments().empty()) {
        return;
    }
    std::unordered_set<std::shared_ptr<Argument>> args_to_delete;
    std::vector<size_t> indices_to_delete;
    auto is_recurse_user = [&](const std::shared_ptr<Instruction> &inst, const size_t idx,
                               const std::shared_ptr<Value> &arg) -> bool {
        auto current_value = arg;
        auto current_inst = inst;
        while (current_inst->get_op() != Operator::CALL && current_inst->users().size() == 1 &&
               std::dynamic_pointer_cast<Instruction>(*current_inst->users().begin())) {
            current_value = current_inst;
            current_inst = (*current_inst->users().begin())->as<Instruction>();
        }
        if (current_inst->get_op() == Operator::CALL) {
            const auto &call = current_inst->as<Call>();
            const auto &current_function = call->get_block()->get_function();
            const auto &called_function = call->get_function()->as<Function>();
            return current_function->get_name() == called_function->get_name() &&
                   call->get_params().at(idx) == current_value;
        }
        return false;
    };
    for (size_t i = 0; i < func->get_arguments().size(); ++i) {
        const auto &arg = func->get_arguments()[i];
        if (arg->users().size() == 0) {
            args_to_delete.insert(arg);
            indices_to_delete.push_back(i);
            continue;
        }
        if (function_analysis_->func_info(func).is_recursive) {
            bool has_normal_user = false;
            for (const auto &user: arg->users()) {
                if (const auto user_inst = std::dynamic_pointer_cast<Instruction>(user)) {
                    if (!is_recurse_user(user_inst, i, arg)) {
                        has_normal_user = true;
                        break;
                    }
                }
            }
            if (!has_normal_user) {
                args_to_delete.insert(arg);
                indices_to_delete.push_back(i);
            }
        }
    }
    for (auto it = func->get_arguments().begin(); it != func->get_arguments().end();) {
        if (args_to_delete.find(*it) != args_to_delete.end()) {
            it = func->get_arguments().erase(it);
        } else {
            ++it;
        }
    }
    func->update_id();
    std::vector<std::shared_ptr<Call>> calls;
    calls.reserve(func->users().size());
    for (const auto &user: func->users()) {
        if (const auto call = std::dynamic_pointer_cast<Call>(user)) {
            calls.push_back(call);
        } else {
            log_fatal("Function can only be used by a call");
        }
    }
    for (const auto &call: calls) {
        auto real_params = call->get_params();
        std::vector<std::shared_ptr<Value>> new_params;
        for (size_t i = 0; i < real_params.size(); ++i) {
            if (std::find(indices_to_delete.begin(), indices_to_delete.end(), i) == indices_to_delete.end()) {
                new_params.push_back(real_params[i]);
            }
        }
        std::shared_ptr<Call> new_call = nullptr;
        if (call->get_name().empty()) {
            new_call = Call::create(func, new_params, nullptr);
        } else {
            new_call = Call::create(call->get_name(), func, new_params, nullptr);
        }
        const auto target_block = call->get_block();
        new_call->set_block(target_block, false);
        call->replace_by_new_value(new_call);
        call->clear_operands();
        auto it = std::find(target_block->get_instructions().begin(), target_block->get_instructions().end(), call);
        if (it == target_block->get_instructions().end()) {
            log_error("%s not found in block %s", call->to_string().c_str(), target_block->get_name().c_str());
        }
        *it = new_call;
    }
    func->update_id();
}

void DeadFuncArgEliminate::transform(const std::shared_ptr<Module> module) {
    function_analysis_ = get_analysis_result<FunctionAnalysis>(module);
    const auto &topo = function_analysis_->topo();
    for (const auto &func: topo) {
        run_on_func(func);
    }
    function_analysis_ = nullptr;
}
}

#include "Pass/Transforms/DCE.h"

using namespace Mir;

namespace {
void add_all_operands(const std::shared_ptr<Instruction> &instruction,
                      std::unordered_set<std::shared_ptr<Instruction>> &set) {
    for (const auto &operand: *instruction) {
        if (const auto inst = operand->is<Instruction>()) {
            set.insert(inst);
        }
    }
}
} // namespace

namespace Pass {
void DeadCodeEliminate::init_useful_instruction(const std::shared_ptr<Function> &function) {
    const auto is_useful_call = [&](const std::shared_ptr<Function> &func) {
        if (func->is_runtime_func()) {
            return func->get_name().find("memset") == std::string::npos;
        }
        if (const auto info = function_analysis_->func_info(func);
            info.io_read || info.io_write || info.memory_write || info.has_side_effect || !info.no_state) {
            return true;
        }
        return false;
    };
    for (const auto &block: function->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (const auto terminator = inst->is<Terminator>()) {
                useful_instructions_.insert(inst);
            } else if (const auto call = inst->is<Call>()) {
                if (const auto called_function = call->get_function()->as<Function>();
                    is_useful_call(called_function)) {
                    useful_instructions_.insert(inst);
                    add_all_operands(call, useful_instructions_);
                }
            }
        }
    }
    for (const auto &arg: function->get_arguments()) {
        if (!arg->get_type()->is_pointer())
            continue;
        for (const auto &user: arg->users()) {
            if (const auto inst = user->is<Instruction>()) {
                useful_instructions_.insert(inst);
            }
        }
    }
}

void DeadCodeEliminate::update_useful_instruction(const std::shared_ptr<Instruction> &instruction) {
    add_all_operands(instruction, useful_instructions_);
    if (instruction->get_type()->is_pointer()) {
        for (const auto &user: instruction->users()) {
            const auto inst = user->is<Instruction>();
            if (inst == nullptr) {
                continue;
            }
            if (const auto op = inst->get_op();
                op == Operator::STORE || op == Operator::GEP || op == Operator::CALL || op == Operator::BITCAST) {
                useful_instructions_.insert(inst);
            } else if (inst->users().size() > 0) {
                useful_instructions_.insert(inst);
            }
        }
    }
}

std::unordered_set<std::shared_ptr<Instruction>>
DeadCodeEliminate::dead_global_variable_eliminate(const std::shared_ptr<Module> &module) {
    for (auto it = module->get_global_variables().begin(); it != module->get_global_variables().end();) {
        if (const auto &gv = *it; gv->users().size() == 0) {
            it = module->get_global_variables().erase(it);
        } else {
            ++it;
        }
    }
    decltype(useful_instructions_) useful_instructions;
    for (const auto &gv: Module::instance()->get_global_variables()) {
        for (const auto &user: gv->users()) {
            const auto inst = user->is<Instruction>();
            if (inst == nullptr) {
                continue;
            }
            if (const auto op = inst->get_op();
                op == Operator::STORE || op == Operator::GEP || op == Operator::CALL || op == Operator::BITCAST) {
                useful_instructions.insert(inst);
            } else if (inst->users().size() > 0) {
                useful_instructions.insert(inst);
            }
        }
    }
    return useful_instructions;
}

void DeadCodeEliminate::run_on_func(const std::shared_ptr<Function> &func,
                                    const std::unordered_set<std::shared_ptr<Instruction>> &initial) {
    useful_instructions_.clear();
    useful_instructions_.insert(initial.begin(), initial.end());
    init_useful_instruction(func);
    bool changed = false;
    do {
        const auto snap = useful_instructions_;
        for (const auto &inst: snap) {
            update_useful_instruction(inst);
        }
        changed = snap.size() != useful_instructions_.size();
    } while (changed);
    for (const auto &block: func->get_blocks()) {
        for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
            if (useful_instructions_.find(*it) == useful_instructions_.end()) {
                (*it)->clear_operands();
                it = block->get_instructions().erase(it);
            } else {
                ++it;
            }
        }
    }
}

void DeadCodeEliminate::transform(const std::shared_ptr<Module> module) {
    function_analysis_ = get_analysis_result<FunctionAnalysis>(module);
    const auto initial_usefuls = dead_global_variable_eliminate(module);
    for (const auto &func: *module) {
        run_on_func(func, initial_usefuls);
    }
    dead_global_variable_eliminate(module);
    function_analysis_ = nullptr;
}

void DeadCodeEliminate::transform(const std::shared_ptr<Function> &func) {
    function_analysis_ = get_analysis_result<FunctionAnalysis>(Module::instance());
    const auto initial_usefuls = dead_global_variable_eliminate(Module::instance());
    run_on_func(func, initial_usefuls);
    dead_global_variable_eliminate(Module::instance());
    function_analysis_ = nullptr;
}
} // namespace Pass

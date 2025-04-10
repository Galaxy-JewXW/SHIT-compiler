#include "Mir/Interpreter.h"
#include "Pass/Transform.h"

static std::shared_ptr<Pass::FunctionAnalysis> func_analysis = nullptr;
using namespace Mir;

namespace {
bool is_constexpr_func(const std::shared_ptr<Function> &function) {
    if (function->is_runtime_func()) {
        return false;
    }
    const auto info = func_analysis->func_info(function);
    if (info.memory_read || info.memory_write) {
        return false;
    }
    if (info.io_read || info.io_write) {
        return false;
    }
    if (info.has_side_effect || !info.no_state) {
        return false;
    }
    if (!info.has_return) {
        return false;
    }
    return true;
}

[[nodiscard]] bool run_on_func(const std::shared_ptr<Function> &function) {
    bool changed = false;
    for (const auto &block: function->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() != Operator::CALL) {
                continue;
            }
            const auto &call_inst = inst->as<Call>();
            const auto &called_function = call_inst->get_function()->as<Function>();
            if (!is_constexpr_func(called_function)) {
                continue;
            }
            const auto &real_params = call_inst->get_params();
            bool all_param_const = true;
            std::vector<eval_t> real_args;
            for (const auto &param: real_params) {
                if (!param->is_constant()) {
                    all_param_const = false;
                    break;
                }
                if (param->get_type()->is_int32()) {
                    real_args.emplace_back(std::any_cast<int>(param->as<ConstInt>()->get_constant_value()));
                } else if (param->get_type()->is_float()) {
                    real_args.emplace_back(std::any_cast<double>(param->as<ConstFloat>()->get_constant_value()));
                }
            }
            if (!all_param_const) {
                continue;
            }
            const auto interpreter = std::make_shared<ConstexprFuncInterpreter>();
            const auto result = interpreter->interpret_function(called_function, real_args);
            if (function->get_return_type()->is_int32()) {
                const auto ret_value = ConstInt::create(static_cast<int>(result));
                call_inst->replace_by_new_value(ret_value);
                changed = true;
            } else if (function->get_return_type()->is_float()) {
                const auto ret_value = ConstFloat::create(static_cast<double>(result));
                call_inst->replace_by_new_value(ret_value);
                changed = true;
            }
        }
    }
    return changed;
}
}

namespace Pass {
void ConstexprFuncEval::transform(const std::shared_ptr<Module> module) {
    func_analysis = create<FunctionAnalysis>();
    func_analysis->run_on(module);
    bool changed = false;
    do {
        changed = false;
        for (const auto &func: *module) {
            changed |= run_on_func(func);
        }
        if (changed) {
            create<GlobalValueNumbering>()->run_on(module);
            create<DeadInstEliminate>()->run_on(module);
        }
    } while (changed);
    create<GlobalValueNumbering>()->run_on(module);
    func_analysis = nullptr;
}
}

#include "Mir/Eval.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Transforms/DCE.h"

using namespace Mir;

namespace Pass {
void ConstexprFuncEval::transform(const std::shared_ptr<Module> module) {
    func_analysis = get_analysis_result<FunctionAnalysis>(module);
    bool changed{false};
    const auto cache{std::make_shared<Interpreter::Cache>()};

    const auto is_constexpr_func = [&](const std::shared_ptr<Function> &func) -> bool {
        if (func->is_runtime_func()) {
            return false;
        }
        const auto info{func_analysis->func_info(func)};
        if (info.memory_read || info.memory_write || info.memory_alloc) {
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
    };

    const auto run_on_func = [&](const std::shared_ptr<Function> &func) -> void {
        for (const auto &block: func->get_blocks()) {
            for (const auto &instruction: block->get_instructions()) {
                if (instruction->get_op() != Operator::CALL) [[likely]] {
                    continue;
                }
                const auto call{instruction->as<Call>()};
                const auto called_func{call->get_function()->as<Function>()};
                if (!is_constexpr_func(called_func)) {
                    continue;
                }

                const auto &real_params{call->get_params()};
                bool all_params_constant{true};
                std::vector<eval_t> real_args;
                for (const auto &param: real_params) {
                    if (!param->is_constant()) {
                        all_params_constant = false;
                        break;
                    }
                    real_args.emplace_back(param->as<Const>()->get_constant_value());
                }
                if (!all_params_constant) {
                    continue;
                }
                eval_t result;
                if (const Interpreter::Key key{called_func->get_name(), real_args}; cache->contains(key)) {
                    result = cache->get(key);
                } else {
                    const auto interpreter{std::make_unique<Interpreter>(cache)};
                    try {
                        interpreter->interpret_function(called_func, real_args);
                    } catch (const std::exception &) {
                        continue;
                    }
                    result = interpreter->frame->ret_value;
                    cache->put(key, result);
                }
                std::shared_ptr<Value> new_value;
                if (func->get_return_type()->is_int32()) {
                    new_value = ConstInt::create(result.get<int>());
                } else if (func->get_return_type()->is_float()) {
                    new_value = ConstFloat::create(result.get<double>());
                } else {
                    log_error("Invalid return type %s", func->get_return_type()->to_string().c_str());
                }
                call->replace_by_new_value(new_value);
                changed = true;
            }
        }
    };

    do {
        changed = false;
        std::for_each(module->get_functions().begin(), module->get_functions().end(), run_on_func);
        if (changed) {
            create<DeadInstEliminate>()->run_on(module);
            create<GlobalValueNumbering>()->run_on(module);
        }
    } while (changed);

    func_analysis = nullptr;
}
}

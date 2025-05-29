#include "Mir/Eval.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Transforms/DCE.h"

using namespace Mir;

namespace Pass {
void ConstexprFuncEval::transform(const std::shared_ptr<Module> module) {
    func_analysis = get_analysis_result<FunctionAnalysis>(module);
    bool dataflow_modified{false}, changed{false};

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
                dataflow_modified = true;
                changed = true;
            }
        }
    };

    do {
        changed = false;
        std::for_each(module->get_functions().begin(), module->get_functions().end(), run_on_func);
    } while (changed);

    if (dataflow_modified) {
        create<GlobalValueNumbering>()->run_on(module);
        create<DeadInstEliminate>()->run_on(module);
    }

    func_analysis = nullptr;
}
}

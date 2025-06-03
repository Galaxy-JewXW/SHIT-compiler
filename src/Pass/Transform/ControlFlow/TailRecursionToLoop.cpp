#include <optional>

#include "Pass/Analyses/FunctionAnalysis.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

// namespace {
// bool is_recursive_call(const std::shared_ptr<Call> &call) {
// }
//
// [[maybe_unused]]
// std::optional<std::shared_ptr<Call>> find_tail_recursive_call(const std::shared_ptr<Block> &block) {
//     const auto size{static_cast<int>(block->get_instructions().size())};
//     for (int i{size - 1}; i >= 0; --i) {
//
//     }
//     return std::nullopt;
// }
// }

void Pass::TailRecursionToLoop::run_on_func(const std::shared_ptr<Function> &func) const {
    const auto &func_data{func_info->func_info(func)};
    if (!func_data.is_recursive) {
        return;
    }
    if (func_data.memory_alloc || func_data.has_side_effect || func_data.memory_write || !func_data.no_state) {
        return;
    }

    auto is_recursive_call = [&](const std::shared_ptr<Call> &call) -> bool {
        return call->get_function() == func;
    };

    [[maybe_unused]]
    auto find_tail_recursive_call = [&](const std::shared_ptr<Block> &block) -> std::optional<std::shared_ptr<Call>> {
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

    [[maybe_unused]]
    auto call_to_loop = [&](const std::shared_ptr<Call> &call) -> void {

    };
}

void Pass::TailRecursionToLoop::transform(const std::shared_ptr<Module> module) {
    func_info = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &func : module->get_functions()) {
        run_on_func(func);
    }
    func_info = nullptr;
}

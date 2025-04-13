#include "Pass/Transform.h"

static std::shared_ptr<Pass::FunctionAnalysis> func_analysis = nullptr;
using namespace Mir;

namespace {
[[maybe_unused]]
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
}

namespace Pass {
void ConstexprFuncEval::transform(const std::shared_ptr<Module> module) {
    func_analysis = create<FunctionAnalysis>();
    func_analysis->run_on(module);
    create<GlobalValueNumbering>()->run_on(module);
    func_analysis = nullptr;
}
}

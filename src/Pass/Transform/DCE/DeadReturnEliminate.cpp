#include "Pass/Transforms/DCE.h"

using namespace Mir;

namespace Pass {
void DeadReturnEliminate::run_on_func(const std::shared_ptr<Function> &func) {
    // 不处理main；不处理返回值已为void的函数
    if (func->get_name() == "main" || func->get_return_type()->is_void()) {
        return;
    }
    bool ret_used = false;
    for (const auto &user: func->users()) {
        if (user->users().size() > 0) {
            ret_used = true;
            break;
        }
    }
    if (ret_used) {
        return;
    }
    func->set_type(Type::Void::void_);
    for (const auto &block: func->get_blocks()) {
        auto &terminator = block->get_instructions().back();
        if (terminator->get_op() != Operator::RET) {
            continue;
        }
        const auto new_return = Ret::create(nullptr);
        new_return->set_block(block, false);
        terminator = new_return;
    }
    for (const auto &user: func->users()) {
        if (user->is<Call>()) {
            user->set_name("");
        }
    }
}

void DeadReturnEliminate::transform(const std::shared_ptr<Module> module) {
    function_analysis_ = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
    function_analysis_ = nullptr;
}
} // namespace Pass

#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
void move_instructions_after_call(const std::shared_ptr<Block> &current_block,
                                  const std::shared_ptr<Block> &next_block,
                                  const std::shared_ptr<Call> &call) {
    auto &current_block_instructions{current_block->get_instructions()};
    const auto call_it = std::find(current_block_instructions.begin(), current_block_instructions.end(), call);
    if (call_it == current_block_instructions.end()) [[unlikely]] {
        log_error("Call not in block");
    }
    for (auto it = std::next(call_it); it != current_block_instructions.end();) {
        (*it)->set_block(next_block);
        it = current_block_instructions.erase(it);
    }
}
}

namespace Pass {
bool Inlining::can_inline(const std::shared_ptr<Function> &func) const {
    const auto &call_graph{func_info->call_graph_func(func)};
    const auto &reverse_call_graph{func_info->call_graph_reverse_func(func)};
    const auto &info{func_info->func_info(func)};
    return call_graph.empty() && !reverse_call_graph.empty() && !info.is_recursive;
}

void Inlining::do_inline(const std::shared_ptr<Function> &func) const {
    std::vector<std::shared_ptr<Call>> calls;
    const auto &reverse_call_graph{func_info->call_graph_reverse_func(func)};

    const auto filter_calls = [&](const std::shared_ptr<Function> &f) -> void {
        for (const auto &block: f->get_blocks()) {
            for (const auto &inst: block->get_instructions()) {
                if (inst->get_op() != Operator::CALL)
                    continue;
                if (const auto call{inst->as<Call>()}; call->get_function() == func) {
                    calls.emplace_back(call);
                }
            }
        }
    };
    std::for_each(reverse_call_graph.begin(), reverse_call_graph.end(), filter_calls);

    for (const auto &call: calls) {
        replace_call(call, call->get_block()->get_function(), func);
    }
}

// 替换调用指令call为内联的函数体
// call: 调用指令
// caller: 调用者函数，即call所位于的函数
// callee: 被call调用的函数
void Inlining::replace_call(const std::shared_ptr<Call> &call,
                            const std::shared_ptr<Function> &caller,
                            const std::shared_ptr<Function> &callee) const {
    const auto current_block{call->get_block()};
    const auto return_type{callee->get_return_type()};

    const auto next_block = Block::create("func.inline", nullptr);
    next_block->set_function(caller, false);
    auto &caller_blocks{caller->get_blocks()};
    caller_blocks.insert(std::next(std::find(caller_blocks.begin(), caller_blocks.end(), current_block)), next_block);

    move_instructions_after_call(current_block, next_block, call);
    for (const auto &child: cfg_info->graph(caller).successors.at(current_block)) {
        for (const auto &inst: child->get_instructions()) {
            if (inst->get_op() != Operator::PHI)
                break;
            if (const auto phi{inst->as<Phi>()}; phi->get_optional_values().count(current_block)) {
                phi->modify_operand(current_block, next_block);
            }
        }
    }

    set_analysis_result_dirty<ControlFlowGraph>(caller);
}

void Inlining::transform(const std::shared_ptr<Module> module) {
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    func_info = get_analysis_result<FunctionAnalysis>(module);

    std::vector<std::shared_ptr<Function>> can_inlined_functions;
    for (const auto &func: module->get_functions()) {
        if (func == module->get_main_function())
            continue;
        if (can_inline(func))
            can_inlined_functions.emplace_back(func);
    }

    for (const auto &func: can_inlined_functions) {
        do_inline(func);
    }

    cfg_info = nullptr;
    func_info = nullptr;
}
}

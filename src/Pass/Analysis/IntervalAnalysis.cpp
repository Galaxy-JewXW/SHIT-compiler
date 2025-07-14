#include <queue>

#include "Pass/Analyses/IntervalAnalysis.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

#define BINARY_CASE(BINARY_TYPE, OP, METHOD, TYPE)                                                                     \
    case BINARY_TYPE::Op::OP: {                                                                                        \
        return std::visit([&](const auto &l, const auto &r) { return TYPE(l).METHOD(TYPE(r)); }, lhs, rhs);            \
    }

namespace {
using Summary = Pass::IntervalAnalysis::Summary;
using AnyIntervalSet = Pass::IntervalAnalysis::AnyIntervalSet;
using IntervalSetInt = Pass::IntervalAnalysis::IntervalSet<int>;
using IntervalSetDouble = Pass::IntervalAnalysis::IntervalSet<double>;
using Context = Pass::IntervalAnalysis::Context;

[[maybe_unused]]
AnyIntervalSet evaluate(const std::shared_ptr<Instruction> &inst, const Context &ctx) {
    switch (inst->get_op()) {
        case Operator::FPTOSI: {
            return std::visit([&](const auto &r) { return IntervalSetInt(r); }, ctx.get(inst));
        }
        case Operator::SITOFP: {
            return std::visit([&](const auto &r) { return IntervalSetDouble(r); }, ctx.get(inst));
        }
        case Operator::INTBINARY: {
            const auto intbinary{inst->as<IntBinary>()};
            const auto lhs{ctx.get(intbinary->get_lhs())}, rhs{ctx.get(intbinary->get_rhs())};
            switch (intbinary->intbinary_op()) {
                BINARY_CASE(IntBinary, ADD, operator+, IntervalSetInt)
                BINARY_CASE(IntBinary, SUB, operator-, IntervalSetInt)
                BINARY_CASE(IntBinary, MUL, operator*, IntervalSetInt)
                BINARY_CASE(IntBinary, DIV, operator/, IntervalSetInt)
                BINARY_CASE(IntBinary, MOD, operator%, IntervalSetInt)
                BINARY_CASE(IntBinary, AND, operator&, IntervalSetInt)
                BINARY_CASE(IntBinary, OR, operator|, IntervalSetInt)
                BINARY_CASE(IntBinary, XOR, operator^, IntervalSetInt)
                BINARY_CASE(IntBinary, SMAX, max, IntervalSetInt)
                BINARY_CASE(IntBinary, SMIN, min, IntervalSetInt)
                default:
                    return IntervalSetInt::make_any();
            }
        }
        case Operator::FLOATBINARY: {
            const auto floatbinary{inst->as<FloatBinary>()};
            const auto lhs{ctx.get(floatbinary->get_lhs())}, rhs{ctx.get(floatbinary->get_rhs())};
            switch (floatbinary->floatbinary_op()) {
                BINARY_CASE(FloatBinary, ADD, operator+, IntervalSetDouble);
                BINARY_CASE(FloatBinary, SUB, operator-, IntervalSetDouble);
                BINARY_CASE(FloatBinary, MUL, operator*, IntervalSetDouble);
                BINARY_CASE(FloatBinary, DIV, operator/, IntervalSetDouble);
                BINARY_CASE(FloatBinary, SMAX, max, IntervalSetDouble)
                BINARY_CASE(FloatBinary, SMIN, min, IntervalSetDouble)
                default:
                    return IntervalSetDouble::make_any();
            }
        }
        case Operator::ICMP:
        case Operator::FCMP: {
            return IntervalSetInt{0, 1};
        }
        case Operator::ZEXT: {
            return ctx.get(inst->as<Zext>()->get_value());
        }
        default:
            break;
    }
    return inst->get_type()->is_float()
               ? AnyIntervalSet(IntervalSetDouble::make_any())
               : AnyIntervalSet(IntervalSetInt::make_any());
}
} // namespace

namespace Pass {
IntervalAnalysis::Summary IntervalAnalysis::rabai_function(const std::shared_ptr<Function> &func) {
    std::unordered_map<std::shared_ptr<Block>, Context> in_ctxs;
    std::unordered_map<std::shared_ptr<Block>, Context> out_ctxs;
    const auto &cfg{cfg_info->graph(func)};
    const auto &loops{loop_info->loops(func)};
    std::queue<std::shared_ptr<Block>> worklist;

    const auto is_back_edge = [&loops](const std::shared_ptr<Block> &b, decltype(b) pred) -> bool {
        for (const auto &loop: loops) {
            if (loop->get_header() != b) {
                continue;
            }
            if (const auto &latchs{loop->get_latch_blocks()};
                std::find_if(latchs.begin(), latchs.end(), [&b, &pred](const auto &latch) {
                    return latch == pred;
                }) == latchs.end()) {
                continue;
            }
            return true;
        }
        return false;
    };

    // 初始化参数的范围
    Context arg_ctx;
    for (const auto &arg: func->get_arguments()) {
        arg_ctx.insert_undefined(arg);
    }
    in_ctxs[func->get_blocks().front()] = std::move(arg_ctx);
    worklist.push(func->get_blocks().front());

    while (!worklist.empty()) {
        const auto current_block{worklist.front()};
        worklist.pop();
    }

    return Summary{};
}

void IntervalAnalysis::analyze(const std::shared_ptr<const Module> module) {
    // 保证对于每一个函数，只有一个返回点
    create<SingleReturnTransform>()->run_on(std::const_pointer_cast<Module>(module));
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    func_info = get_analysis_result<FunctionAnalysis>(module);
    loop_info = get_analysis_result<LoopAnalysis>(module);

    auto topo{func_info->topo()};
    std::reverse(topo.begin(), topo.end());
    std::unordered_set worklist(topo.begin(), topo.end());

    SummaryManager summary_manager;

    while (!worklist.empty()) {
        const auto func{*worklist.begin()};
        worklist.erase(worklist.begin());
        const auto old_summary{summary_manager.get(func)};
        const auto new_summary = rabai_function(func);
        summary_manager.update(func, new_summary);
        if (old_summary != new_summary) {
            for (const auto &g: func_info->call_graph_reverse_func(func)) {
                if (worklist.find(g) == worklist.end()) {
                    worklist.insert(g);
                }
            }
        }
    }

    cfg_info = nullptr;
    func_info = nullptr;
    loop_info = nullptr;
}
} // namespace Pass

#undef BINARY_CASE

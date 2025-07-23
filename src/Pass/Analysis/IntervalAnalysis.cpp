#include <queue>

#include "Pass/Analyses/IntervalAnalysis.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
using SummaryManager = Pass::IntervalAnalysis::SummaryManager;
using AnyIntervalSet = Pass::IntervalAnalysis::AnyIntervalSet;
using IntervalSetInt = Pass::IntervalAnalysis::IntervalSet<int>;
using IntervalSetDouble = Pass::IntervalAnalysis::IntervalSet<double>;
using Context = Pass::IntervalAnalysis::Context;

void evaluate(const std::shared_ptr<Instruction> &inst, Context &ctx, const SummaryManager &summary_manager) {
    if (inst->is<Terminator>() || inst->get_op() == Operator::PHI) {
        return;
    }
    AnyIntervalSet result_interval;
    switch (inst->get_op()) {
        case Operator::INTBINARY: {
            const auto &intbinary{inst->as<IntBinary>()};
            const auto lhs{ctx.get(intbinary->get_lhs())}, rhs{ctx.get(intbinary->get_rhs())};
            result_interval = std::visit(
                    [&](const auto &a, const auto &b) {
                        const auto l{IntervalSetInt(a)}, r{IntervalSetInt(b)};
                        switch (intbinary->intbinary_op()) {
                            case IntBinary::Op::ADD:
                                return l + r;
                            case IntBinary::Op::SUB:
                                return l - r;
                            case IntBinary::Op::MUL:
                                return l * r;
                            case IntBinary::Op::DIV:
                                return l / r;
                            case IntBinary::Op::MOD:
                                return l % r;
                            case IntBinary::Op::AND:
                                return l & r;
                            case IntBinary::Op::OR:
                                return l | r;
                            case IntBinary::Op::XOR:
                                return l ^ r;
                            case IntBinary::Op::SMAX:
                                return l.max(r);
                            case IntBinary::Op::SMIN:
                                return l.min(r);
                            default:
                                return IntervalSetInt::make_any();
                        }
                    },
                    lhs, rhs);
            break;
        }
        case Operator::FLOATBINARY: {
            const auto &floatbinary{inst->as<FloatBinary>()};
            const auto lhs{ctx.get(floatbinary->get_lhs())}, rhs{ctx.get(floatbinary->get_rhs())};
            result_interval = std::visit(
                    [&](const auto &a, const auto &b) {
                        const auto l{IntervalSetDouble(a)}, r{IntervalSetDouble(b)};
                        switch (floatbinary->floatbinary_op()) {
                            case FloatBinary::Op::ADD:
                                return l + r;
                            case FloatBinary::Op::SUB:
                                return l - r;
                            case FloatBinary::Op::MUL:
                                return l * r;
                            case FloatBinary::Op::DIV:
                                return l / r;
                            case FloatBinary::Op::SMAX:
                                return l.max(r);
                            case FloatBinary::Op::SMIN:
                                return l.min(r);
                            default:
                                return IntervalSetDouble::make_any();
                        }
                    },
                    lhs, rhs);
            break;
        }
        case Operator::FLOATTERNARY: {
            const auto &floatternary{inst->as<FloatTernary>()};
            const auto x1{ctx.get(floatternary->get_x())}, x2{ctx.get(floatternary->get_y())},
                    x3{ctx.get(floatternary->get_z())};
            result_interval = std::visit(
                    [&](const auto &a, const auto &b, const auto &c) {
                        const auto x{IntervalSetDouble(a)}, y{IntervalSetDouble(b)}, z{IntervalSetDouble(c)};
                        switch (floatternary->op) {
                            case FloatTernary::Op::FMADD:
                                return x * y + z;
                            case FloatTernary::Op::FNMADD:
                                return -(x * y + z);
                            case FloatTernary::Op::FMSUB:
                                return x * y - z;
                            case FloatTernary::Op::FNMSUB:
                                return -(x * y - z);
                            default:
                                return IntervalSetDouble::make_any();
                        }
                    },
                    x1, x2, x3);
            break;
        }
        case Operator::FNEG: {
            result_interval = std::visit([](const auto &a) { return IntervalSetDouble(-a); },
                                         ctx.get(inst->as<FNeg>()->get_value()));
            break;
        }
        case Operator::ICMP:
        case Operator::FCMP: {
            result_interval = IntervalSetInt{0, 1};
            break;
        }
        case Operator::SITOFP: {
            result_interval = std::visit([](const auto &a) { return IntervalSetDouble(a); },
                                         ctx.get(inst->as<Sitofp>()->get_value()));
            break;
        }
        case Operator::FPTOSI: {
            result_interval = std::visit([](const auto &a) { return IntervalSetInt(a); },
                                         ctx.get(inst->as<Fptosi>()->get_value()));
            break;
        }
        case Operator::ZEXT: {
            result_interval =
                    std::visit([](const auto &a) { return IntervalSetInt(a); }, ctx.get(inst->as<Zext>()->get_value()));
            break;
        }
        case Operator::CALL: {
            if (const auto func{inst->as<Call>()->get_function()->as<Function>()}; func->is_runtime_func()) {
                if (const auto name{func->get_name()}; name == "getch") {
                    result_interval = IntervalSetInt{-128, 127};
                } else if (name == "getint") {
                    result_interval = IntervalSetInt::make_any();
                } else if (name == "getfloat") {
                    result_interval = IntervalSetDouble::make_any();
                } else if (name == "getarray" || name == "getfarray") {
                    result_interval = IntervalSetInt{0, Pass::numeric_limits_v<int>::infinity};
                } else if (func->get_return_type()->is_float()) {
                    result_interval = IntervalSetDouble::make_any();
                } else {
                    result_interval = IntervalSetInt::make_any();
                }
            } else {
                // TODO: summary的处理
                const auto summary = summary_manager.get(func);
                if (func->get_return_type()->is_float()) {
                    result_interval = IntervalSetDouble::make_any();
                } else {
                    result_interval = IntervalSetInt::make_any();
                }
            }
            break;
        }
        default: {
            if (inst->get_type()->is_void()) {
                return;
            }
            if (inst->get_type()->is_float()) {
                result_interval = IntervalSetDouble::make_any();
            } else {
                result_interval = IntervalSetInt::make_any();
            }
        }
    }
    if (!inst->get_type()->is_void()) {
        ctx.insert(inst, result_interval);
    }
}
} // namespace

namespace Pass {
IntervalAnalysis::Summary IntervalAnalysis::rabai_function(const std::shared_ptr<Function> &func,
                                                           const SummaryManager &summary_manager) const {
    std::unordered_map<std::shared_ptr<Block>, Context> in_ctxs;
    std::unordered_map<std::shared_ptr<Block>, Context> out_ctxs;
    for (const auto &block: func->get_blocks()) {
        in_ctxs.emplace(block, Context{});
        out_ctxs.emplace(block, Context{});
    }

    const auto &loops{loop_info->loops(func)};
    std::queue<std::shared_ptr<Block>> worklist;
    std::unordered_set<std::shared_ptr<Block>> worklist_set;

    const auto is_back_edge = [&loops](const std::shared_ptr<Block> &b, decltype(b) pred) -> bool {
        for (const auto &loop: loops) {
            if (loop->get_header() != b) {
                continue;
            }
            if (const auto &latchs{loop->get_latch_blocks()};
                std::find_if(latchs.begin(), latchs.end(), [&pred](const auto &latch) { return latch == pred; }) ==
                latchs.end()) {
                continue;
            }
            return true;
        }
        return false;
    };

    const auto refine_context = [](const std::shared_ptr<Value> &cond, const bool is_true_branch, Context &ctx) {
        const auto icmp{cond->is<Icmp>()};
        if (icmp == nullptr) {
            return;
        }
        if (!(!icmp->get_lhs()->is_constant() && icmp->get_rhs()->is_constant())) {
            return;
        }
        auto lhs{std::get<IntervalSetInt>(ctx.get(icmp->get_lhs()))};
        const auto rhs{**icmp->get_rhs()->as<ConstInt>()};
        const auto interval = [&]() -> IntervalSetInt {
            switch (icmp->op) {
                case Icmp::Op::EQ: {
                    if (is_true_branch) {
                        return IntervalSetInt{rhs};
                    }
                    return IntervalSetInt{numeric_limits_v<int>::neg_infinity, rhs - 1}.union_with(
                            IntervalSetInt{rhs + 1, numeric_limits_v<int>::infinity});
                }
                case Icmp::Op::NE: {
                    if (is_true_branch) {
                        return IntervalSetInt{numeric_limits_v<int>::neg_infinity, rhs - 1}.union_with(
                                IntervalSetInt{rhs + 1, numeric_limits_v<int>::infinity});
                    }
                    return IntervalSetInt{rhs};
                }
                case Icmp::Op::LT: {
                    if (is_true_branch) {
                        return IntervalSetInt{numeric_limits_v<int>::neg_infinity, rhs - 1};
                    }
                    return IntervalSetInt{rhs, numeric_limits_v<int>::infinity};
                }
                case Icmp::Op::LE: {
                    if (is_true_branch) {
                        return IntervalSetInt{numeric_limits_v<int>::neg_infinity, rhs};
                    }
                    return IntervalSetInt{rhs + 1, numeric_limits_v<int>::infinity};
                }
                case Icmp::Op::GT: {
                    if (is_true_branch) {
                        return IntervalSetInt{rhs + 1, numeric_limits_v<int>::infinity};
                    }
                    return IntervalSetInt{numeric_limits_v<int>::neg_infinity, rhs};
                }
                case Icmp::Op::GE: {
                    if (is_true_branch) {
                        return IntervalSetInt{rhs, numeric_limits_v<int>::infinity};
                    }
                    return IntervalSetInt{numeric_limits_v<int>::neg_infinity, rhs - 1};
                }
            }
            log_error("Should not reach here");
        }();
        lhs = lhs.intersect_with(interval);
        ctx.insert(icmp->get_lhs(), lhs);
    };

    const auto propagate = [&](const std::shared_ptr<Block> &pred, decltype(pred) succ, const Context &ctx) {
        const auto old_in_succ{in_ctxs[succ]};
        auto new_in_succ = old_in_succ;

        if (is_back_edge(succ, pred)) {
            new_in_succ = new_in_succ.widen(ctx);
        } else {
            new_in_succ = new_in_succ.union_with(ctx);
        }
        if (new_in_succ != old_in_succ || worklist_set.find(succ) == worklist_set.end()) {
            in_ctxs[succ] = new_in_succ;
            worklist.push(succ);
            worklist_set.insert(succ);
        }
    };

    // 初始化参数的范围
    const auto &entry{func->get_blocks().front()};
    Context arg_ctx;
    for (const auto &arg: func->get_arguments()) {
        arg_ctx.insert_undefined(arg);
    }
    in_ctxs[entry] = std::move(arg_ctx);
    worklist.push(func->get_blocks().front());

    while (!worklist.empty()) {
        const auto current_block{worklist.front()};
        worklist.pop();
        out_ctxs[current_block] = in_ctxs[current_block];
        for (const auto &inst: current_block->get_instructions()) {
            evaluate(inst, out_ctxs[current_block], summary_manager);
        }
        const auto &terminator{current_block->get_instructions().back()};
        if (const auto branch{terminator->is<Branch>()}) {
            const auto &true_block{branch->get_true_block()}, &false_block{branch->get_false_block()};
            const auto &cond{branch->get_cond()};
            auto true_context{out_ctxs[current_block]}, false_context{out_ctxs[current_block]};
            refine_context(cond, true, true_context);
            refine_context(cond, false, false_context);
            propagate(current_block, true_block, true_context);
            propagate(current_block, false_block, false_context);
        } else if (const auto switch_{terminator->is<Switch>()}) {
            // 处理所有case分支
            for (const auto &[value, block]: switch_->cases()) {
                auto case_context{out_ctxs[current_block]};
                auto interval = std::get<IntervalSetInt>(case_context.get(switch_->get_base()));
                interval = interval.intersect_with(IntervalSetInt{**value->as<ConstInt>()});
                case_context.insert(switch_->get_base(), interval);
                propagate(current_block, block, case_context);
            }
            // 处理default分支
            auto default_context{out_ctxs[current_block]};
            auto interval = std::get<IntervalSetInt>(default_context.get(switch_->get_base()));
            for (const auto &[value, block]: switch_->cases()) {
                interval = interval.difference(IntervalSetInt{**value->as<ConstInt>()});
            }
            default_context.insert(switch_->get_base(), interval);
            propagate(current_block, switch_->get_default_block(), default_context);
        } else if (const auto jump{terminator->is<Jump>()}) {
            propagate(current_block, jump->get_target_block(), out_ctxs[current_block]);
        }
    }

    // std::cout << "=== Interval Analysis Results for Function: " << func->get_name() << " ===" << std::endl;
    // for (const auto &block: func->get_blocks()) {
    //     std::cout << "\nBlock: " << block->get_name() << std::endl;
    //     std::cout << "  In Context:" << std::endl;
    //     const auto &in_ctx = in_ctxs[block];
    //     std::cout << in_ctx.to_string() << std::endl;
    //     std::cout << "  Out Context:" << std::endl;
    //     const auto &out_ctx = out_ctxs[block];
    //     std::cout << out_ctx.to_string() << std::endl;
    // }
    // std::cout << "=== End of Analysis ===\n" << std::endl;

    return Summary{};
}

void IntervalAnalysis::analyze(const std::shared_ptr<const Module> module) {
    // 保证对于每一个函数，只有一个返回点
    create<StandardizeBinary>()->run_on(std::const_pointer_cast<Module>(module));
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
        const auto new_summary = rabai_function(func, summary_manager);
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

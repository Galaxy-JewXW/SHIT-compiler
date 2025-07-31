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
                const auto summary = summary_manager.get(func);
                if (func->get_return_type()->is_float()) {
                    result_interval = std::get<IntervalSetDouble>(summary);
                } else {
                    result_interval = std::get<IntervalSetInt>(summary);
                }
            }
            break;
        }
        case Operator::MOVE: {
            const auto move = inst->as<Move>();
            const auto &src = move->get_from_value(), &dst = move->get_to_value();
            ctx.insert(dst, ctx.get(src));
            return;
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
IntervalAnalysis::AnyIntervalSet IntervalAnalysis::rabai_function(const std::shared_ptr<Function> &func,
                                                                  const SummaryManager &summary_manager) {
    std::unordered_map<std::shared_ptr<Block>, Context> in_ctxs;
    std::unordered_map<std::shared_ptr<Block>, Context> out_ctxs;
    std::unordered_map<std::shared_ptr<Ret>, AnyIntervalSet> ret_ctxs;

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

    const auto propagate = [&](const std::shared_ptr<Block> &pred, decltype(pred) succ, const Context &pred_out_ctx) {
        const auto old_in_succ{in_ctxs[succ]};
        auto new_in_succ = old_in_succ;
        new_in_succ = new_in_succ.union_with(pred_out_ctx);

        bool changed{false};
        for (const auto &inst: succ->get_instructions()) {
            if (inst->get_op() != Operator::PHI)
                break;
            const auto phi{inst->as<Phi>()};
            if (phi->get_optional_values().count(pred) == 0)
                continue;
            const auto incoming_value{phi->get_optional_values().at(pred)};
            const auto incoming_interval = pred_out_ctx.get(incoming_value);
            const auto old_phi_interval = new_in_succ.get(phi);
            AnyIntervalSet new_phi_interval = std::visit(
                    [&](const auto &old_v) -> AnyIntervalSet {
                        return std::visit(
                                [&](const auto &incoming_v) -> AnyIntervalSet {
                                    if constexpr (std::is_same_v<decltype(old_v), decltype(incoming_v)>) {
                                        if (is_back_edge(succ, pred)) {
                                            auto old_copy = old_v;
                                            return old_copy.widen(incoming_v);
                                        }
                                        auto old_copy = old_v;
                                        return old_copy.union_with(incoming_v);
                                    } else {
                                        log_error("Type mismatch in PHI node! old: %s, incoming: %s",
                                                  old_v.to_string().c_str(), incoming_v.to_string().c_str());
                                    }
                                }, incoming_interval);
                    }, old_phi_interval);
            if (new_phi_interval != old_phi_interval) {
                new_in_succ.insert(phi, new_phi_interval);
                changed = true;
            }
        }

        if (changed || worklist_set.find(succ) == worklist_set.end()) {
            in_ctxs[succ] = new_in_succ;
            worklist.push(succ);
            worklist_set.insert(succ);
        }
    };

    // 初始化参数的范围
    const auto &entry{func->get_blocks().front()};
    Context arg_ctx;
    for (const auto &arg: func->get_arguments()) {
        arg_ctx.insert_top(arg);
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
        switch (const auto terminator{current_block->get_instructions().back()};
            terminator->get_op()) {
            case Operator::BRANCH: {
                const auto branch = terminator->as<Branch>();
                const auto &true_block{branch->get_true_block()}, &false_block{branch->get_false_block()};
                const auto &cond{branch->get_cond()};
                auto true_context{out_ctxs[current_block]}, false_context{out_ctxs[current_block]};
                refine_context(cond, true, true_context);
                refine_context(cond, false, false_context);
                propagate(current_block, true_block, true_context);
                propagate(current_block, false_block, false_context);
                break;
            }
            case Operator::SWITCH: {
                const auto switch_{terminator->as<Switch>()};
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
                break;
            }
            case Operator::JUMP: {
                const auto jump = terminator->as<Jump>();
                propagate(current_block, jump->get_target_block(), out_ctxs[current_block]);
                break;
            }
            case Operator::RET: {
                const auto ret = terminator->as<Ret>();
                if (func->get_return_type()->is_void())
                    break;

                AnyIntervalSet interval_set;
                if (const auto inst = ret->get_value()->is<Instruction>()) {
                    interval_set = out_ctxs[current_block].get(inst);
                } else if (const auto constant = ret->get_value()->is<Const>()) {
                    interval_set = std::visit([](const auto x) -> AnyIntervalSet {
                        return IntervalSet<std::decay_t<decltype(x)>>{x};
                    }, constant->get_constant_value());
                } else {
                    // set to all
                    if (func->get_return_type()->is_int32()) {
                        interval_set = IntervalSetInt::make_any();
                    } else if (func->get_return_type()->is_float()) {
                        interval_set = IntervalSetDouble::make_any();
                    } else {
                        log_error("Invalid type");
                    }
                }
                ret_ctxs[ret] = interval_set;
                break;
            }
            default:
                break;
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

    for (const auto &[b, ctx]: in_ctxs) {
        block_in_ctxs.insert({b.get(), ctx});
    }

    if (func->get_return_type()->is_int32()) {
        IntervalSet<int> return_intervals;
        for (const auto &[ret, interval]: ret_ctxs) {
            return_intervals = return_intervals.union_with(std::get<decltype(return_intervals)>(interval));
        }
        return return_intervals;
    }
    if (func->get_return_type()->is_float()) {
        IntervalSet<double> return_intervals;
        for (const auto &[ret, interval]: ret_ctxs) {
            return_intervals = return_intervals.union_with(std::get<decltype(return_intervals)>(interval));
        }
        return return_intervals;
    }
    return IntervalSetInt::make_undefined();
}

Context IntervalAnalysis::ctx_after(const std::shared_ptr<Instruction> &inst, const std::shared_ptr<Block> &block) {
    const auto cache_key = std::pair{inst.get(), block.get()};
    if (const auto cache_it = after_ctx_cache_.find(cache_key); cache_it != after_ctx_cache_.end()) {
        return cache_it->second;
    }
    const auto it{block_in_ctxs.find(block.get())};
    if (it == block_in_ctxs.end()) [[unlikely]] {
        log_error("Unfound block: %s", block->to_string().c_str());
    }
    Context current_ctx{it->second};
    bool flag{false};
    for (const auto &i: block->get_instructions()) {
        if (flag)
            break;
        if (i == inst)
            flag = true;
        evaluate(i, current_ctx, summary_manager);
    }
    const auto &[inserted_it, success] = after_ctx_cache_.emplace(cache_key, current_ctx);
    return inserted_it->second;
}

void IntervalAnalysis::analyze(const std::shared_ptr<const Module> module) {
    block_in_ctxs.clear();
    func_info = nullptr;
    loop_info = nullptr;
    summary_manager = SummaryManager{};

    // 保证对于每一个函数，只有一个返回点
    create<StandardizeBinary>()->run_on(std::const_pointer_cast<Module>(module));
    // create<SingleReturnTransform>()->run_on(std::const_pointer_cast<Module>(module));
    func_info = get_analysis_result<FunctionAnalysis>(module);
    loop_info = get_analysis_result<LoopAnalysis>(module);

    auto topo{func_info->topo()};
    std::reverse(topo.begin(), topo.end());
    std::unordered_set worklist(topo.begin(), topo.end());
    for (const auto &func: module->get_functions()) {
        if (!worklist.count(func))
            worklist.insert(func);
    }

    summary_manager.clear();
    while (!worklist.empty()) {
        const auto func{*worklist.begin()};
        worklist.erase(worklist.begin());
        const auto old_summary{summary_manager.get(func)};
        const auto new_summary = rabai_function(func, summary_manager);
        if (func->get_return_type()->is_void()) {
            continue;
        }
        summary_manager.update(func, new_summary);
        if (old_summary != new_summary) {
            for (const auto &g: func_info->call_graph_reverse_func(func)) {
                if (worklist.find(g) == worklist.end()) {
                    worklist.insert(g);
                }
            }
        }
    }

    // for (const auto &[func, any_summary] : summary_manager.get_summaries()) {
    //     if (func->get_return_type()->is_void())
    //         continue;
    //     if (func->get_return_type()->is_int32()) {
    //         const auto summary = std::get<IntervalSetInt>(any_summary);
    //         log_debug("\n%s\n%s", func->get_name().c_str(), summary.to_string().c_str());
    //     } else if (func->get_return_type()->is_float()) {
    //         const auto summary = std::get<IntervalSetDouble>(any_summary);
    //         log_debug("\n%s\n%s", func->get_name().c_str(), summary.to_string().c_str());
    //     }
    // }

    func_info = nullptr;
    loop_info = nullptr;
}
} // namespace Pass

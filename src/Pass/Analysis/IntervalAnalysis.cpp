#include <array>
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

    const auto is_back_edge = [&loops](const std::shared_ptr<Block> &b, const std::shared_ptr<Block> &pred) -> bool {
        for (const auto &loop: loops) {
            if (loop->get_header() != b) {
                continue;
            }
            if (const auto &latchs{loop->get_latch_blocks()};
                std::find_if(latchs.begin(), latchs.end(), [&pred](const auto &latch) { return latch == pred; }) !=
                latchs.end()) {
                return true;
            }
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
        const auto lhs_var = icmp->get_lhs();
        const auto lhs_interval_any = ctx.get(lhs_var);
        if (!std::holds_alternative<IntervalSet<int>>(lhs_interval_any)) {
            return;
        }
        auto lhs{std::get<IntervalSet<int>>(lhs_interval_any)};
        const auto rhs{**icmp->get_rhs()->as<ConstInt>()};
        const auto interval = [&]() -> IntervalSet<int> {
            switch (icmp->op) {
                case Icmp::Op::EQ:
                    return is_true_branch ? IntervalSet{rhs}
                                          : IntervalSet{numeric_limits_v<int>::neg_infinity, rhs - 1}.union_with(
                                                    IntervalSet{rhs + 1, numeric_limits_v<int>::infinity});
                case Icmp::Op::NE:
                    return !is_true_branch ? IntervalSet{rhs}
                                           : IntervalSet{numeric_limits_v<int>::neg_infinity, rhs - 1}.union_with(
                                                     IntervalSet{rhs + 1, numeric_limits_v<int>::infinity});
                case Icmp::Op::LT:
                    return is_true_branch ? IntervalSet{numeric_limits_v<int>::neg_infinity, rhs - 1}
                                          : IntervalSet{rhs, numeric_limits_v<int>::infinity};
                case Icmp::Op::LE:
                    return is_true_branch ? IntervalSet{numeric_limits_v<int>::neg_infinity, rhs}
                                          : IntervalSet{rhs + 1, numeric_limits_v<int>::infinity};
                case Icmp::Op::GT:
                    return is_true_branch ? IntervalSet{rhs + 1, numeric_limits_v<int>::infinity}
                                          : IntervalSet{numeric_limits_v<int>::neg_infinity, rhs};
                case Icmp::Op::GE:
                    return is_true_branch ? IntervalSet{rhs, numeric_limits_v<int>::infinity}
                                          : IntervalSet{numeric_limits_v<int>::neg_infinity, rhs - 1};
                default:
                    log_error("Unsupported ICMP operator for refinement");
            }
        }();
        lhs.intersect_with(interval);
        ctx.insert(lhs_var, lhs);
    };

    const auto &entry{func->get_blocks().front()};
    Context arg_ctx;
    for (const auto &arg: func->get_arguments()) {
        arg_ctx.insert_top(arg);
    }
    in_ctxs[entry] = std::move(arg_ctx);
    worklist.push(func->get_blocks().front());
    worklist_set.insert(func->get_blocks().front());

    while (!worklist.empty()) {
        const auto current_block{worklist.front()};
        worklist.pop();
        worklist_set.erase(current_block);

        // 1. 计算当前块的 Out 上下文
        auto &current_out_ctx = out_ctxs[current_block];
        current_out_ctx = in_ctxs[current_block];
        for (const auto &inst: current_block->get_instructions()) {
            if (inst->is<Terminator>() || inst->get_op() == Operator::PHI)
                continue;
            evaluate(inst, current_out_ctx, summary_manager);
        }

        // 2. 将结果传播到所有后继
        switch (const auto terminator{current_block->get_instructions().back()}; terminator->get_op()) {
            case Operator::JUMP: {
                const auto jump = terminator->as<Jump>();
                const auto &succ = jump->get_target_block();

                auto &succ_in_ctx = in_ctxs[succ];
                Context old_succ_in_ctx = succ_in_ctx;

                // 2a. 合并前驱上下文
                succ_in_ctx.union_with(current_out_ctx);

                // 2b. 单独处理Phi节点，使用未精化的值
                for (const auto &inst: succ->get_instructions()) {
                    if (auto phi = inst->is<Phi>()) {
                        if (phi->get_optional_values().count(current_block)) {
                            const auto &incoming_value = phi->get_optional_values().at(current_block);
                            const auto incoming_interval = current_out_ctx.get(incoming_value);
                            auto old_phi_interval = old_succ_in_ctx.get(phi);

                            AnyIntervalSet new_phi_interval = std::visit(
                                    [&](auto &old_v, const auto &incoming_v) -> AnyIntervalSet {
                                        if constexpr (std::is_same_v<std::decay_t<decltype(old_v)>,
                                                                     std::decay_t<decltype(incoming_v)>>) {
                                            if (is_back_edge(succ, current_block)) {
                                                old_v.widen(incoming_v);
                                            } else {
                                                old_v.union_with(incoming_v);
                                            }
                                            return old_v;
                                        }
                                        log_error("Type mismatch for PHI node in JUMP handler.");
                                    },
                                    old_phi_interval, incoming_interval);
                            succ_in_ctx.insert(phi, new_phi_interval);
                        }
                    } else {
                        break;
                    }
                }

                // 2c. JUMP无条件，无需精化。检查变化并加入worklist
                if (succ_in_ctx != old_succ_in_ctx) {
                    worklist.push(succ);
                    if (worklist_set.find(succ) == worklist_set.end()) {
                        worklist_set.insert(succ);
                    }
                }
                break;
            }
            case Operator::BRANCH: {
                const auto branch = terminator->as<Branch>();
                const auto &cond{branch->get_cond()};
                std::array<std::shared_ptr<Block>, 2> successors{branch->get_true_block(), branch->get_false_block()};

                for (size_t i = 0; i < successors.size(); ++i) {
                    bool is_true_path = (i == 0);
                    const auto &succ = successors[i];

                    auto &succ_in_ctx = in_ctxs[succ];
                    Context old_succ_in_ctx = succ_in_ctx;

                    // 2a. 合并前驱上下文
                    succ_in_ctx.union_with(current_out_ctx);

                    // 2b. 单独处理Phi节点
                    for (const auto &inst: succ->get_instructions()) {
                        if (auto phi = inst->is<Phi>()) {
                            if (phi->get_optional_values().count(current_block)) {
                                const auto &incoming_value = phi->get_optional_values().at(current_block);
                                const auto incoming_interval = current_out_ctx.get(incoming_value);

                                auto old_phi_interval = old_succ_in_ctx.get(phi);
                                AnyIntervalSet new_phi_interval = std::visit(
                                        [&](auto &old_v, const auto &incoming_v) -> AnyIntervalSet {
                                            if constexpr (std::is_same_v<std::decay_t<decltype(old_v)>,
                                                                         std::decay_t<decltype(incoming_v)>>) {
                                                if (is_back_edge(succ, current_block)) {
                                                    old_v.widen(incoming_v);
                                                } else {
                                                    old_v.union_with(incoming_v);
                                                }
                                                return old_v;
                                            }
                                            log_error("Type mismatch for PHI node in BRANCH handler.");
                                        },
                                        old_phi_interval, incoming_interval);
                                succ_in_ctx.insert(phi, new_phi_interval);
                            }
                        } else {
                            break;
                        }
                    }
                    // 2c. 在合并和phi更新完成后，进行精化
                    refine_context(cond, is_true_path, succ_in_ctx);
                    // 2d. 检查变化
                    if (succ_in_ctx != old_succ_in_ctx) {
                        worklist.push(succ);
                        if (worklist_set.find(succ) == worklist_set.end()) {
                            worklist_set.insert(succ);
                        }
                    }
                }
                break;
            }
            case Operator::SWITCH: {
                log_error("Not supported");
            }
            case Operator::RET: {
                const auto ret = terminator->as<Ret>();
                if (func->get_return_type()->is_void())
                    break;

                AnyIntervalSet interval_set = current_out_ctx.get(ret->get_value());
                if (ret_ctxs.find(ret) == ret_ctxs.end()) {
                    ret_ctxs[ret] = interval_set;
                } else {
                    std::visit(
                            [&](auto &old_set, const auto &new_set) {
                                if constexpr (std::is_same_v<std::decay_t<decltype(old_set)>,
                                                             std::decay_t<decltype(new_set)>>) {
                                    old_set.union_with(new_set);
                                }
                            },
                            ret_ctxs[ret], interval_set);
                }
                break;
            }
            default:
                // 对于其他没有后继的终结符(如Unreachable)，什么都不做
                break;
        }
    }

    std::cout << "=== Interval Analysis Results for Function: " << func->get_name() << " ===" << std::endl;
    for (const auto &block: func->get_blocks()) {
        std::cout << "\nBlock: " << block->get_name() << std::endl;
        std::cout << "  In Context:" << std::endl;
        const auto &in_ctx = in_ctxs[block];
        std::cout << in_ctx.to_string() << std::endl;
        std::cout << "  Out Context:" << std::endl;
        const auto &out_ctx = out_ctxs[block];
        std::cout << out_ctx.to_string() << std::endl;
    }
    std::cout << "=== End of Analysis ===\n" << std::endl;

    for (const auto &[b, ctx]: in_ctxs) {
        block_in_ctxs.insert({b.get(), ctx});
    }

    if (func->get_return_type()->is_int32()) {
        IntervalSet<int> return_intervals;
        for (const auto &[ret, interval]: ret_ctxs) {
            return_intervals.union_with(std::get<decltype(return_intervals)>(interval));
        }
        return return_intervals;
    }
    if (func->get_return_type()->is_float()) {
        IntervalSet<double> return_intervals;
        for (const auto &[ret, interval]: ret_ctxs) {
            return_intervals.union_with(std::get<decltype(return_intervals)>(interval));
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

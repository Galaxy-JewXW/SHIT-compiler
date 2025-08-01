#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transforms/Loop.h"
#include "Pass/Analyses/SCEVAnalysis.h"

namespace Pass {
    void InductionVariables::transform(std::shared_ptr<Mir::Module> module) {
        this->scev_info_ = get_analysis_result<SCEVAnalysis>(module);
        this->loop_info_ = get_analysis_result<LoopAnalysis>(module);
        for (auto &function : *module) {
            run_on(function);
        }
    }

    void InductionVariables::transform(const std::shared_ptr<Mir::Function> &function) {
        for (auto loop_node : this->loop_info_->loop_forest(function)) {
            run(loop_node);
        }
    }

    void InductionVariables::run(std::shared_ptr<LoopNodeTreeNode> &loop_node) {
        for (auto child_node : loop_node->get_children()) {
            run(child_node);
        }

        if( get_trip_count(loop_node->get_loop()) ) {
            //TODO: 这个优化 lcssa phi 的步骤也可以选做
        }
    }

    bool InductionVariables::get_trip_count(std::shared_ptr<Loop> loop) {
        if (loop->get_trip_count() >= 0 ) return true;

        if(loop->get_exits().size() > 1) return false;
        auto instr = loop->get_header()->get_instructions().back();
        if (!instr->is<Mir::Branch>()) return false;
        auto br = instr->as<Mir::Branch>();
        auto cond = br->get_cond();
        if (!cond->is<Mir::Icmp>()) return false;

        auto cond_instr = cond->as<Mir::Icmp>();
        auto lhs = cond_instr->get_lhs();
        auto rhs = cond_instr->get_rhs();

        if (lhs->is<Mir::Phi>() && rhs->is<Mir::ConstInt>()) {
            if(!this->scev_info_->query(lhs)) return false;
            int n = rhs->as<Mir::ConstInt>()->get_constant_value().get<int>();

            int ans = get_tick_num(this->scev_info_->query(lhs), cond_instr->icmp_op(), n);
            loop->set_trip_count(ans);
            return ans != -1;
        }
        else if (rhs->is<Mir::Phi>() && lhs->is<Mir::ConstInt>()) {
            if(!this->scev_info_->query(rhs)) return false;
            int n = lhs->as<Mir::ConstInt>()->get_constant_value().get<int>();

            int ans = get_tick_num(this->scev_info_->query(rhs), Mir::Icmp::swap_op(cond_instr->icmp_op()), n);
            loop->set_trip_count(ans);
            return ans != -1;
        }

        return false;
    }

    int InductionVariables::get_tick_num(std::shared_ptr<SCEVExpr> scev_expr, Mir::Icmp::Op op, int n) {
        if (scev_expr->not_negative() && op != Mir::Icmp::Op::EQ && op != Mir::Icmp::Op::NE) {
            int init = scev_expr->get_init();
            int step = scev_expr->get_step();
            switch(op) {
                case Mir::Icmp::Op::LT: {
                    if (init > n) return -1;
                    return static_cast<int>(std::floor(static_cast<double>(n - init) / step)) + 1;
                }
                case Mir::Icmp::Op::LE: {
                    if (init >= n) return -1;
                    return static_cast<int>(std::ceil(static_cast<double>(n - init) / step));
                }
                case Mir::Icmp::Op::GT: {
                    if (init <= n) return -1;
                    return static_cast<int>(std::ceil(static_cast<double>(n - init) / step));
                }
                case Mir::Icmp::Op::GE:{
                    if (init < n) {
                        return -1;
                    }
                    return static_cast<int>(std::floor(static_cast<double>(n - init) / step)) + 1;
                }
             }
        }

        return -1;
        // 关于 eq 与 ne 的情况 可以后续补充
    }

}
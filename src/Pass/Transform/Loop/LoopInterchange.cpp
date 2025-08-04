#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transforms/Loop.h"
#include "Pass/Analyses/SCEVAnalysis.h"

namespace Pass {

    void LoopInterchange::transform(std::shared_ptr<Mir::Module> module) {
        const auto loop_info = get_analysis_result<LoopAnalysis>(module);
        const auto scev_info = get_analysis_result<SCEVAnalysis>(module);
        loop_info_ = loop_info;
        scev_info_ = scev_info;
        for(auto &func : *module) {
            run_on(func);
        }
    }

    void LoopInterchange::run_on(const std::shared_ptr<Mir::Function> &function) {
        std::vector<std::shared_ptr<LoopNodeTreeNode>> loop_nests;
        for (auto &loop_node : loop_info_->loop_forest(function)) {
            if (loop_node->is_nest()) loop_nests.push_back(loop_node);
        }

        for (auto &loop_nest : loop_nests) {
            if (check_on_nest(loop_nest)) {
                //transform_on_nest(loop_nest);
            }
        }
    }

    bool LoopInterchange::check_on_nest(const std::shared_ptr<LoopNodeTreeNode>& loop_nest) {
        std::vector<std::shared_ptr<LoopNodeTreeNode>> loop_nodes;
        this->get_loops(loop_nest, loop_nodes);

        if (loop_nodes.size() < this->min_nest_depth || loop_nodes.size() > this->max_nest_depth) return false;
        for (auto &loop_node : loop_nodes) if(!is_computable(loop_node)) return false;

        if(!get_dependence_info(loop_nodes)) return false;
        if (loop_nodes[0]->get_loop()->get_exits().size() != 1) return false;

        get_cache_cost_manager(loop_nodes);
        return true;
    }

    bool LoopInterchange::is_computable(const std::shared_ptr<LoopNodeTreeNode> &loop_node) {
        auto loop = loop_node->get_loop();
        auto instr = loop->get_header()->get_instructions().back();
        if (!instr->is<Mir::Branch>()) return false;
        auto br = instr->as<Mir::Branch>();
        auto cond = br->get_cond();
        if (!cond->is<Mir::Icmp>()) return false;
        auto cond_instr = cond->as<Mir::Icmp>();

        if(loop_node->def_value(cond_instr->get_lhs())) {
            if (!this->scev_info_->query(cond_instr->get_lhs())) return false;
            if (!cond_instr->get_lhs()->is<Mir::Phi>()) return false;
        }
        else if (loop_node->def_value(cond_instr->get_rhs())) {
            if (!this->scev_info_->query(cond_instr->get_rhs())) return false;
            if (!cond_instr->get_rhs()->is<Mir::Phi>()) return false;
        }
        //这里是在分析 回边是否可计算，不过可能不够严谨，先这样写

        if(loop_node->get_loop()->get_exitings().size() != 1) return false;
        //这里需要同时保证回边数与 exiting_block 数都为 1，但回边数经 loop simply 保证
        return true;
    }

    bool LoopInterchange::get_dependence_info(std::vector<std::shared_ptr<LoopNodeTreeNode>> &loops) {
        //TODO:分析依赖关系
    }

    void LoopInterchange::get_cache_cost_manager(std::vector<std::shared_ptr<LoopNodeTreeNode>> &loops) {
        //TODO:这里需要实现
    }

    void transform_on_nest(const std::shared_ptr<LoopNodeTreeNode>& loop_nest) {

    }

    void LoopInterchange::get_loops(const std::shared_ptr<LoopNodeTreeNode>& loop_nest,
                                    std::vector<std::shared_ptr<LoopNodeTreeNode>> &loops) {
        auto loop_node = loop_nest;
        while (loop_node) {
            if (loop_node) {
                loops.push_back(loop_node);
            }
            loop_node = loop_node->get_children()[0];
        }
    }



}
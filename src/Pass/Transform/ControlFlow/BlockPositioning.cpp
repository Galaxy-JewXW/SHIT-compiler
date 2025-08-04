#include "Pass/Analyses/BranchProbabilityAnalysis.h"
#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
[[maybe_unused]]
void reverse_postorder_placement(const std::shared_ptr<Function> &func,
                                 const Pass::ControlFlowGraph::Graph &graph) {
    auto &blocks = func->get_blocks();
    std::vector<std::shared_ptr<Block>> rpo;
    std::unordered_set<std::shared_ptr<Block>> visited;

    auto dfs = [&](auto &&self, const std::shared_ptr<Block> &block) -> void {
        visited.insert(block);
        for (const auto &succ: graph.successors.at(block)) {
            if (!visited.count(succ)) {
                self(self, succ);
            }
        }
        rpo.push_back(block);
    };

    const auto entry = func->get_blocks().front();
    dfs(dfs, entry);
    std::reverse(rpo.begin(), rpo.end());
    blocks = std::move(rpo);
}

[[maybe_unused]]
void static_probability_placement(const std::shared_ptr<Function> &func,
                                  const std::shared_ptr<Pass::ControlFlowGraph> &cfg,
                                  const std::shared_ptr<Pass::BranchProbabilityAnalysis> &branch_prob) {
    const auto blocks_snap{func->get_blocks()};
    const auto entry{func->get_blocks().front()};
    const auto &edge_prob{branch_prob->edges_prob(func.get())};
    const auto &graph{cfg->graph(func)};

    // log_debug("\n%s", func->to_string().c_str());

    std::unordered_set<std::shared_ptr<Block>> placed;
    const auto build_chain = [&placed, &edge_prob, &graph](const std::shared_ptr<Block> &block)
        -> std::vector<std::shared_ptr<Block>> {
        if (placed.count(block)) {
            return {};
        }
        std::vector<std::shared_ptr<Block>> _chain;
        auto current_block = block;
        while (current_block != nullptr && !placed.count(current_block)) {
            _chain.push_back(current_block);
            placed.insert(current_block);
            decltype(current_block) hot_succ{nullptr};
            double max_prob{-1.0};
            for (const auto &succ: graph.successors.at(current_block)) {
                const auto p = edge_prob.at(
                        Pass::BranchProbabilityAnalysis::Edge::make_edge(current_block.get(), succ.get()));
                // log_debug("%s -> %s : %lf", current_block->get_name().c_str(), succ->get_name().c_str(), p);
                if (p > max_prob) {
                    max_prob = p;
                    hot_succ = succ;
                }
            }
            if (hot_succ != nullptr && !placed.count(hot_succ) && max_prob >= 0.5) {
                current_block = hot_succ;
            } else {
                current_block = nullptr;
            }
        }
        return _chain;
    };

    std::vector<std::shared_ptr<Block>> chain = build_chain(entry);
    for (const auto &block: blocks_snap) {
        if (placed.count(block))
            continue;
        const auto cur_chain = build_chain(block);
        chain.insert(chain.end(), cur_chain.begin(), cur_chain.end());
    }

    func->get_blocks() = std::move(chain);
}
}

namespace Pass {
template<typename T>
void BlockPositioning<T>::do_reverse_postorder_placement(const std::shared_ptr<Module> &module) {
    set_analysis_result_dirty<ControlFlowGraph>(module);
    const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);
    for (const auto &func: module->get_functions()) {
        reverse_postorder_placement(func, cfg_info->graph(func));
    }
}

template<typename T>
void BlockPositioning<T>::do_static_probability_placement(const std::shared_ptr<Module> &module) {
    set_analysis_result_dirty<ControlFlowGraph>(module);
    const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);
    const auto branch_prob_info = get_analysis_result<BranchProbabilityAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        static_probability_placement(func, cfg_info, branch_prob_info);
    }
}

template class BlockPositioning<O0_Type>;
template class BlockPositioning<O1_Type>;
}
#include "Pass/Analyses/BranchProbabilityAnalysis.h"

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Analyses/LoopAnalysis.h"

using namespace Mir;

#define MAKE_EDGE(src, dst) (Edge::make_edge(src, dst))

namespace {
using Edge = Pass::BranchProbabilityAnalysis::Edge;

class BranchProbabilityImpl {
public:
    BranchProbabilityImpl(const std::shared_ptr<Function> &current_function,
                          const Pass::ControlFlowGraph::Graph &cfg_graph,
                          const Pass::DominanceGraph::Graph &dom_graph,
                          const std::shared_ptr<Pass::LoopAnalysis> &loop_info,
                          std::unordered_map<Edge, double, Edge::Hash> &edge_probability,
                          std::unordered_map<const Block *, double> &block_probability) :
        current_function(current_function), cfg_graph(cfg_graph),
        dom_graph(dom_graph), loop_info(loop_info),
        edge_probability(edge_probability), block_probability(block_probability) {}

private:
    const std::shared_ptr<Function> &current_function;
    const Pass::ControlFlowGraph::Graph &cfg_graph;
    const Pass::DominanceGraph::Graph &dom_graph;
    const std::shared_ptr<Pass::LoopAnalysis> &loop_info;
    std::unordered_map<Edge, double, Edge::Hash> &edge_probability;
    std::unordered_map<const Block *, double> &block_probability;

    // weights
    static constexpr int BACKEDGE_TAKEN_WEIGHT = 124;
    static constexpr int BACKEDGE_NOTTAKEN_WEIGHT = 4;

    static constexpr int MAX_WEIGHT = 2048;

public:
    void impl();
};

void BranchProbabilityImpl::impl() {
    // 初始化权重
    for (const auto &_blk: current_function->get_blocks()) {
        const auto block = _blk.get();
        switch (const auto &terminator = block->get_instructions().back();
            terminator->get_op()) {
            case Operator::JUMP: {
                const auto target = terminator->as<Jump>()->get_target_block().get();
                MAKE_EDGE(block, target).weight = MAX_WEIGHT;
            }
            default:
                break;
        }
    }
}
}

namespace Pass {
void BranchProbabilityAnalysis::analyze(const std::shared_ptr<const Module> module) {
    const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);
    const auto dom_info = get_analysis_result<DominanceGraph>(module);
    const auto loop_info = get_analysis_result<LoopAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        BranchProbabilityImpl impl{func, cfg_info->graph(func), dom_info->graph(func), loop_info,
                                   edge_probabilities[func.get()], block_probabilities[func.get()]};
        impl.impl();
    }
}
}

#undef MAKE_EDGE

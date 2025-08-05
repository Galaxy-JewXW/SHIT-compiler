#include "Pass/Analyses/BranchProbabilityAnalysis.h"

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Analyses/IntervalAnalysis.h"
#include "Pass/Analyses/LoopAnalysis.h"
#include "Pass/Transforms/Common.h"

using namespace Mir;

#define MAKE_EDGE(src, dst) (Edge::make_edge(src, dst))

namespace {
bool is_exiting_loop(const std::shared_ptr<Block> &block, const std::shared_ptr<Pass::Loop> &loop) {
    const auto &exits{loop->get_exits()};
    return std::find(exits.begin(), exits.end(), block) != exits.end();
}

using Edge = Pass::BranchProbabilityAnalysis::Edge;

class BranchProbabilityImpl {
public:
    BranchProbabilityImpl(const std::shared_ptr<Function> &current_function,
                          const Pass::ControlFlowGraph::Graph &cfg_graph,
                          const std::shared_ptr<Pass::LoopAnalysis> &loop_info,
                          const std::shared_ptr<Pass::IntervalAnalysis> &interval_info,
                          const Pass::DominanceGraph::Graph &dom_graph,
                          std::unordered_map<Edge, double, Edge::Hash> &edge_probability,
                          std::unordered_map<const Block *, double> &block_probability) :
        current_function(current_function), cfg_graph(cfg_graph),
        loop_info(loop_info), interval_info(interval_info), dom_graph(dom_graph),
        edge_probability(edge_probability), block_probability(block_probability) {}

private:
    const std::shared_ptr<Function> &current_function;
    const Pass::ControlFlowGraph::Graph &cfg_graph;
    const std::shared_ptr<Pass::LoopAnalysis> &loop_info;
    const std::shared_ptr<Pass::IntervalAnalysis> &interval_info;
    const Pass::DominanceGraph::Graph &dom_graph;
    std::unordered_map<Edge, double, Edge::Hash> &edge_probability;
    std::unordered_map<const Block *, double> &block_probability;

    // weights
    static constexpr int BACKEDGE_TAKEN_WEIGHT = 124;
    static constexpr int BACKEDGE_NOTTAKEN_WEIGHT = 4;

    static constexpr int BRANCH_TAKEN_WEIGHT = 20;
    static constexpr int BRANCH_NOTTAKEN_WEIGHT = 12;

    static constexpr int MAX_WEIGHT = 2048;
    static constexpr double CONVERGENCE_THRESHOLD = 1e-6;

    static constexpr int MAX_ITER = 100;

    void calc_branch(const Branch *branch) const;

public:
    void impl() const;
};

void BranchProbabilityImpl::calc_branch(const Branch *const branch) const {
    const auto true_block{branch->get_true_block()}, false_block{branch->get_false_block()};
    const auto current_block{branch->get_block()};
    // 回边的概率一般要比退出边的概率高很多
    if (const auto loop_node = loop_info->find_block_in_forest(current_function, current_block)) {
        const auto loop = loop_node->get_loop();
        if (const auto &exitings = loop->get_exitings();
            std::find(exitings.begin(), exitings.end(), current_block) != loop->get_exits().end()) {
            const bool flag1 = is_exiting_loop(false_block, loop), flag2 = is_exiting_loop(true_block, loop);
            if (flag1 && flag2) {
                if (dom_graph.dominated_blocks.at(true_block).size() > dom_graph.dominated_blocks.at(false_block).size()) {
                    MAKE_EDGE(current_block, true_block).weight = 25;
                    MAKE_EDGE(current_block, false_block).weight = 7;
                } else {
                    MAKE_EDGE(current_block, true_block).weight = 7;
                    MAKE_EDGE(current_block, false_block).weight = 25;
                }
                return;
            }
            if (flag1) {
                MAKE_EDGE(current_block, true_block).weight = BACKEDGE_TAKEN_WEIGHT;
                MAKE_EDGE(current_block, false_block).weight = BACKEDGE_NOTTAKEN_WEIGHT;
                return;
            }
            if (flag2) {
                MAKE_EDGE(current_block, true_block).weight = BACKEDGE_NOTTAKEN_WEIGHT;
                MAKE_EDGE(current_block, false_block).weight = BACKEDGE_TAKEN_WEIGHT;
                return;
            }
        }
    }
    const auto cond{branch->get_cond()};
    if (const auto icmp{cond->is<Icmp>()}) {
        if (icmp->get_rhs()->is_constant()) {
            if (const auto rhs{**icmp->get_rhs()->as<ConstInt>()}; rhs == 0) {
                switch (icmp->icmp_op()) {
                    case Icmp::Op::EQ:
                    case Icmp::Op::LE:
                    case Icmp::Op::LT: {
                        MAKE_EDGE(current_block, true_block).weight = BRANCH_NOTTAKEN_WEIGHT;
                        MAKE_EDGE(current_block, false_block).weight = BRANCH_TAKEN_WEIGHT;
                        return;
                    }
                    case Icmp::Op::GT:
                    case Icmp::Op::GE:
                    case Icmp::Op::NE: {
                        MAKE_EDGE(current_block, true_block).weight = BRANCH_TAKEN_WEIGHT;
                        MAKE_EDGE(current_block, false_block).weight = BRANCH_NOTTAKEN_WEIGHT;
                        return;
                    }
                    default:
                        break;
                }
            } else if (rhs == -1) {
                switch (icmp->icmp_op()) {
                    case Icmp::Op::EQ: {
                        MAKE_EDGE(current_block, true_block).weight = BRANCH_NOTTAKEN_WEIGHT;
                        MAKE_EDGE(current_block, false_block).weight = BRANCH_TAKEN_WEIGHT;
                        return;
                    }
                    case Icmp::Op::NE: {
                        MAKE_EDGE(current_block, true_block).weight = BRANCH_TAKEN_WEIGHT;
                        MAKE_EDGE(current_block, false_block).weight = BRANCH_NOTTAKEN_WEIGHT;
                        return;
                    }
                    default:
                        break;
                }
            }
        }
    } else if (const auto fcmp{cond->is<Fcmp>()}) {
        if (const auto type{fcmp->fcmp_op()}; type == Fcmp::Op::EQ) {
            MAKE_EDGE(current_block, true_block).weight = BRANCH_NOTTAKEN_WEIGHT;
            MAKE_EDGE(current_block, false_block).weight = BRANCH_TAKEN_WEIGHT;
            return;
        } else if (type == Fcmp::Op::NE) {
            MAKE_EDGE(current_block, true_block).weight = BRANCH_TAKEN_WEIGHT;
            MAKE_EDGE(current_block, false_block).weight = BRANCH_NOTTAKEN_WEIGHT;
            return;
        }
    }
    if (dom_graph.dominated_blocks.at(true_block).size() > dom_graph.dominated_blocks.at(false_block).size()) {
        MAKE_EDGE(current_block, true_block).weight = 20;
        MAKE_EDGE(current_block, false_block).weight = 12;
    } else {
        MAKE_EDGE(current_block, true_block).weight = 12;
        MAKE_EDGE(current_block, false_block).weight = 20;
    }
}

void BranchProbabilityImpl::impl() const {
    // current_function->update_id();
    // log_debug("\n%s", current_function->to_string().c_str());
    // 初始化权重
    for (const auto &_blk: current_function->get_blocks()) {
        const auto block = _blk.get();
        block_probability[block] = 0.0;
        switch (const auto &terminator = block->get_instructions().back();
            terminator->get_op()) {
            case Operator::JUMP: {
                const auto target = terminator->as<Jump>()->get_target_block().get();
                MAKE_EDGE(block, target).weight = MAX_WEIGHT;
                break;
            }
            case Operator::BRANCH: {
                // log_debug("%s", terminator->to_string().c_str());
                calc_branch(terminator->as<Branch>().get());
                break;
            }
            case Operator::SWITCH: {
                const auto switch_inst = terminator->as<Switch>();
                for (const auto &[v, b]: switch_inst->cases()) {
                    MAKE_EDGE(block, b.get()).weight = BRANCH_TAKEN_WEIGHT;
                }
                MAKE_EDGE(block, switch_inst->get_default_block().get()).weight = BRANCH_NOTTAKEN_WEIGHT;
                break;
            }
            default:
                break;
        }
    }

    block_probability[current_function->get_blocks().front().get()] = 1.0;

    std::vector<std::shared_ptr<Block>> rpo;
    std::unordered_set<std::shared_ptr<Block>> visited;

    const auto dfs = [&](auto &&self, const std::shared_ptr<Block> &block) -> void {
        visited.insert(block);
        for (const auto &succ: cfg_graph.successors.at(block)) {
            if (!visited.count(succ)) {
                self(self, succ);
            }
        }
        rpo.push_back(block);
    };

    dfs(dfs, current_function->get_blocks().front());
    std::reverse(rpo.begin(), rpo.end());

    const auto entry = current_function->get_blocks().front().get();
    bool changed;
    int cnt{0};
    do {
        changed = false;
        for (const auto &_blk: rpo) {
            const auto block = _blk.get();
            // 对于非入口块，根据其前驱的频率更新自己的频率
            if (block != entry) {
                double new_freq{0.0};
                for (const auto &_prev: cfg_graph.predecessors.at(_blk)) {
                    new_freq += edge_probability[MAKE_EDGE(_prev.get(), block)] * block_probability[_prev.get()];
                }
                if (std::abs(new_freq - block_probability[block]) > CONVERGENCE_THRESHOLD) {
                    changed = true;
                }
                block_probability[block] = new_freq;
            }
            // 根据当前块的出边权重，更新其所有出边的条件跳转概率
            if (!cfg_graph.successors.at(_blk).empty()) {
                int total_weight{0};
                for (const auto &_succ: cfg_graph.successors.at(_blk)) {
                    total_weight += MAKE_EDGE(block, _succ.get()).weight;
                }
                for (const auto &_succ: cfg_graph.successors.at(_blk)) {
                    if (total_weight == 0) [[unlikely]] {
                        edge_probability[MAKE_EDGE(block, _succ.get())] =
                                1.0 / static_cast<int>(cfg_graph.successors.at(_blk).size());
                    } else {
                        edge_probability[MAKE_EDGE(block, _succ.get())] =
                                static_cast<double>(MAKE_EDGE(block, _succ.get()).weight) / total_weight;
                    }
                }
            }
        }
        ++cnt;
    } while (changed && cnt < MAX_ITER);
}
}

namespace Pass {
void BranchProbabilityAnalysis::analyze(const std::shared_ptr<const Module> module) {
    edge_probabilities.clear();
    block_probabilities.clear();

    create<StandardizeBinary>()->run_on(std::const_pointer_cast<Module>(module));
    // module->update_id();
    // log_debug("%s", module->to_string().c_str());
    const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);
    const auto dom_info = get_analysis_result<DominanceGraph>(module);
    const auto loop_info = get_analysis_result<LoopAnalysis>(module);
    // module->update_id();
    // log_debug("%s", module->to_string().c_str());
    for (const auto &func: module->get_functions()) {
        BranchProbabilityImpl impl{func, cfg_info->graph(func), loop_info, nullptr, dom_info->graph(func),
                                   edge_probabilities[func.get()], block_probabilities[func.get()]};
        impl.impl();
    }
}
}

#undef MAKE_EDGE

#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
// class Scheduler {
//     const Pass::DominanceGraph::Graph &graph;
//
//     std::unordered_map<std::shared_ptr<Block>, std::unordered_set<std::shared_ptr<Instruction>>> &outs;
//
// public:
//     Scheduler(const Pass::DominanceGraph::Graph &graph,
//               std::unordered_map<std::shared_ptr<Block>, std::unordered_set<std::shared_ptr<Instruction>>> &outs) :
//         graph(graph), outs(outs) {}
//
//     void schedule(const std::shared_ptr<Block> &block);
// };
//
// void Scheduler::schedule(const std::shared_ptr<Block> &block) {
//     const auto &terminator{block->get_instructions().back()};
//     for (const auto &operand : terminator->get_operands()) {
//         if (const auto i{operand->is<Instruction>()})
//             outs[block].insert(i);
//     }
//     for (const auto &child : graph.dominance_children.at(block)) {
//         schedule(child);
//         outs[block].insert(outs[child].begin(), outs[child].end());
//     }
// }
} // namespace

namespace Pass {
void InstSchedule::run_on_func(const std::shared_ptr<Function> &func) const {
    std::unordered_map<std::shared_ptr<Block>, std::unordered_set<std::shared_ptr<Instruction>>> out_live_variables;
    for (const auto &block: func->get_blocks()) {
        out_live_variables[block] = {};
    }
    for (const auto &block: func->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() != Operator::PHI)
                break;
            const auto phi{inst->as<Phi>()};
            for (const auto &[b, v]: phi->get_optional_values()) {
                if (const auto i{v->is<Instruction>()})
                    out_live_variables[b].insert(i);
            }
        }
    }

    const auto &graph{dom_graph->graph(func)};
    auto dfs = [&](auto &&self, const std::shared_ptr<Block> &block) -> void {
        const auto &terminator{block->get_instructions().back()};
        for (const auto &operand: terminator->get_operands()) {
            if (const auto i{operand->is<Instruction>()})
                out_live_variables[block].insert(i);
        }
        for (const auto &child: graph.dominance_children.at(block)) {
            self(self, child);
            out_live_variables[block].insert(out_live_variables[child].begin(), out_live_variables[child].end());
        }
    };
    dfs(dfs, func->get_blocks().front());
}

void InstSchedule::transform(const std::shared_ptr<Module> module) {
    dom_graph = get_analysis_result<DominanceGraph>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    dom_graph = nullptr;
}
} // namespace Pass

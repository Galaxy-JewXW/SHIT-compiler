#include <functional>
#include <queue>

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Mir/Instruction.h"
#include "Pass/Util.h"
#include "Pass/Analyses/DominanceGraph.h"

using FunctionPtr = std::shared_ptr<Mir::Function>;
using BlockPtr = std::shared_ptr<Mir::Block>;

namespace {
// 构建函数中每个基本块的前驱和后继映射
void build_predecessors_successors(const FunctionPtr &func,
                                   std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &pred_map,
                                   std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &succ_map) {
    for (const auto &block: func->get_blocks()) {
        const auto last_instruction = block->get_instructions().back();
        const auto terminator = std::dynamic_pointer_cast<Mir::Terminator>(last_instruction);
        if (terminator == nullptr) {
            log_error("Last instruction of block %s is not a terminator: %s", block->get_name().c_str(),
                      last_instruction->to_string().c_str());
        }
        std::unordered_set<BlockPtr> successors;
        if (terminator->get_op() == Mir::Operator::BRANCH) {
            const auto branch = std::static_pointer_cast<Mir::Branch>(terminator);
            successors.insert(branch->get_true_block());
            successors.insert(branch->get_false_block());
        } else if (terminator->get_op() == Mir::Operator::JUMP) {
            const auto jump = std::static_pointer_cast<Mir::Jump>(terminator);
            successors.insert(jump->get_target_block());
        } else if (terminator->get_op() != Mir::Operator::RET) {
            log_error("Last instruction of block %s is not a terminator: %s", block->get_name().c_str(),
                      last_instruction->to_string().c_str());
        }
        for (const auto &successor: successors) {
            succ_map[block].insert(successor);
            pred_map[successor].insert(block);
        }
    }
    std::ostringstream oss;
    oss << "\n▷▷ Function: [" << func->get_name() << "]\n";
    for (const auto &block: func->get_blocks()) {
        const auto &preds = pred_map[block],
                   &succs = succ_map[block];
        oss << "  ■ Block: \"" << block->get_name() << "\"\n"
                << "    ├─←←← " << Pass::Utils::format_blocks(preds) << "\n"
                << "    └─→→→ " << Pass::Utils::format_blocks(succs) << "\n";
    }
    log_debug("%s", oss.str().c_str());
}

[[maybe_unused]]
void build_post_order(const FunctionPtr &func,
                      const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominance_children_map,
                      std::vector<BlockPtr> &post_order) {
    std::unordered_set<BlockPtr> visited;
    std::function<void(const BlockPtr &)> dfs = [&](const BlockPtr &block) {
        if (visited.count(block)) {
            return;
        }
        visited.insert(block);
        for (const auto &child: dominance_children_map.at(block)) {
            dfs(child);
        }
        post_order.push_back(block);
    };
    dfs(func->get_blocks().front());
}

[[maybe_unused]]
void build_dom_tree_layer_order(const FunctionPtr &func,
                                const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dom_children_map,
                                std::vector<BlockPtr> &dom_tree_layer_order) {
    std::unordered_set<BlockPtr> visited;
    std::queue<BlockPtr> queue;

    const auto entry = func->get_blocks().front();
    queue.push(entry);
    visited.insert(entry);

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();
        dom_tree_layer_order.push_back(current);
        if (dom_children_map.count(current)) {
            for (const auto &child: dom_children_map.at(current)) {
                if (!visited.count(child)) {
                    queue.push(child);
                    visited.insert(child);
                }
            }
        }
    }
}
}

namespace Pass {
void ControlFlowGraph::analyze(const std::shared_ptr<const Mir::Module> module) {
    for (const auto &func: *module) {
        dirty_funcs_.try_emplace(func, true);
    }
    for (const auto &func: *module) {
        if (!dirty_funcs_[func]) {
            continue;
        }
        if (graphs_.count(func)) {
            graphs_.erase(func);
        }
        graphs_.try_emplace(func, Graph{});
        auto &pred_map = graphs_[func].predecessors,
             &succ_map = graphs_[func].successors;
        build_predecessors_successors(func, pred_map, succ_map);
        dirty_funcs_[func] = false;
    }
}

bool ControlFlowGraph::is_dirty() const {
    return std::any_of(dirty_funcs_.begin(), dirty_funcs_.end(), [](const auto &pair) {
        return pair.second;
    });
}

void ControlFlowGraph::set_dirty(const FunctionPtr &func) {
    if (dirty_funcs_[func]) {
        return;
    }
    dirty_funcs_[func] = true;
    set_analysis_result_dirty<DominanceGraph>(func);
}
}

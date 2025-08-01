#include <queue>

#include "Mir/Instruction.h"
#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Util.h"
#include "Pass/Analyses/LoopAnalysis.h"

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
        if (const auto t = terminator->get_op(); t == Mir::Operator::BRANCH) {
            const auto branch = std::static_pointer_cast<Mir::Branch>(terminator);
            successors.insert(branch->get_true_block());
            successors.insert(branch->get_false_block());
        } else if (t == Mir::Operator::JUMP) {
            const auto jump = std::static_pointer_cast<Mir::Jump>(terminator);
            successors.insert(jump->get_target_block());
        } else if (t == Mir::Operator::SWITCH) {
            const auto switch_ = std::static_pointer_cast<Mir::Switch>(terminator);
            successors.insert(switch_->get_default_block());
            for (const auto &[value, block]: switch_->cases()) {
                successors.insert(block);
            }
        } else if (t != Mir::Operator::RET) {
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
        const auto &preds = pred_map[block], &succs = succ_map[block];
        oss << "  ■ Block: \"" << block->get_name() << "\"\n"
                << "    ├─←←← " << Pass::Utils::format_blocks(preds) << "\n"
                << "    └─→→→ " << Pass::Utils::format_blocks(succs) << "\n";
    }
    log_trace("%s", oss.str().c_str());
}

[[maybe_unused]]
void build_post_order(const FunctionPtr &func,
                      const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominance_children_map,
                      std::vector<BlockPtr> &post_order) {
    std::unordered_set<BlockPtr> visited;
    auto dfs = [&](auto &&self, const BlockPtr &block) -> void {
        if (visited.count(block)) {
            return;
        }
        visited.insert(block);
        for (const auto &child: dominance_children_map.at(block)) {
            self(self, child);
        }
        post_order.push_back(block);
    };
    dfs(dfs, func->get_blocks().front());
}
} // namespace

namespace Pass {
void ControlFlowGraph::analyze(const std::shared_ptr<const Mir::Module> module) {
    if (const auto func_size = module->get_functions().size();
        func_size != dirty_funcs_.size() || func_size != graphs_.size()) {
        // 部分函数被删除了
        graphs_.clear();
        dirty_funcs_.clear();
    }
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
        auto &pred_map = graphs_[func].predecessors, &succ_map = graphs_[func].successors;
        build_predecessors_successors(func, pred_map, succ_map);
        dirty_funcs_[func] = false;
    }
}

bool ControlFlowGraph::is_dirty() const {
    return std::any_of(dirty_funcs_.begin(), dirty_funcs_.end(), [](const auto &pair) { return pair.second; });
}

bool ControlFlowGraph::is_dirty(const std::shared_ptr<Mir::Function> &function) const {
    return dirty_funcs_.at(function);
}

void ControlFlowGraph::set_dirty(const FunctionPtr &func) {
    if (dirty_funcs_[func]) {
        return;
    }
    dirty_funcs_[func] = true;
    set_analysis_result_dirty<DominanceGraph>(func);
    set_analysis_result_dirty<LoopAnalysis>(func);
}

std::vector<BlockPtr> ControlFlowGraph::reverse_post_order(const FunctionPtr &func) const {
    std::vector<BlockPtr> post_order;
    std::unordered_set<BlockPtr> visited;
    auto dfs = [&](auto &&self, const BlockPtr &block) -> void {
        if (visited.count(block)) {
            return;
        }
        visited.insert(block);
        for (const auto &child: graphs_.at(func).successors.at(block)) {
            self(self, child);
        }
        post_order.push_back(block);
    };
    dfs(dfs, func->get_blocks().front());
    std::reverse(post_order.begin(), post_order.end());
    if (post_order.size() != func->get_blocks().size()) {
        log_error("Unexpected error");
    }
    return post_order;
}
} // namespace Pass

#include <functional>
#include <numeric>

#include "Mir/Instruction.h"
#include "Pass/Analysis.h"

using FunctionPtr = std::shared_ptr<Mir::Function>;
using BlockPtr = std::shared_ptr<Mir::Block>;

namespace {
// 输出基本块集合的辅助方法
std::string format_blocks(const std::unordered_set<BlockPtr> &blocks) {
    if (blocks.empty()) return "∅";
    std::vector<std::string> names;
    names.reserve(blocks.size());
    for (const auto &b: blocks) names.push_back("\'" + b->get_name() + "\'");
    return std::accumulate(
        std::next(names.begin()), names.end(), names[0],
        [](const std::string &a, const std::string &b) { return a + ", " + b; }
    );
}

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
                << "    ├─←←← " << format_blocks(preds) << "\n"
                << "    └─→→→ " << format_blocks(succs) << "\n";
    }
    log_debug("%s", oss.str().c_str());
}

// 构建每个基本块的支配集（dominator）以及被支配集（dominated）
void build_dominators_dominated(const FunctionPtr &func,
                                const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &pred_map,
                                std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominator,
                                std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominated) {
    const auto &blocks = func->get_blocks();
    const std::unordered_set all_blocks(blocks.begin(), blocks.end());
    for (const auto &block: blocks) {
        if (block == blocks.front())
            dominator[block] = {block};
        else
            dominator[block] = all_blocks;
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &block: blocks) {
            if (block == blocks.front())
                continue;
            // 新支配集为其所有前驱支配集的交集，再加上自身
            std::unordered_set<BlockPtr> new_dom;
            bool first = true;
            for (const auto &pred: pred_map.at(block)) {
                if (first) {
                    new_dom = dominator.at(pred);
                    first = false;
                } else {
                    std::unordered_set<BlockPtr> temp;
                    for (const auto &d: new_dom) {
                        if (dominator.at(pred).count(d))
                            temp.insert(d);
                    }
                    new_dom = std::move(temp);
                }
            }
            new_dom.insert(block);
            if (new_dom != dominator[block]) {
                dominator[block] = std::move(new_dom);
                changed = true;
            }
        }
    }
    for (const auto &b: blocks) {
        for (const auto &c: blocks) {
            if (dominator.at(c).count(b))
                dominated[b].insert(c);
        }
    }
    std::ostringstream oss;
    oss << "\n▷▷ Dominated blocks for function: [" << func->get_name() << "]\n";
    for (const auto &block: func->get_blocks()) {
        const auto &dominated_blocks = dominated[block];
        oss << "  ■ Block: \"" << block->get_name() << "\"\n"
                << "    └─ Dominates: " << format_blocks(dominated_blocks) << "\n";
    }
    log_debug("%s", oss.str().c_str());
}

[[deprecated("Use Tarjan instead"), maybe_unused]]
void build_immediate_dominators(const FunctionPtr &func,
                                const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominator,
                                std::unordered_map<BlockPtr, BlockPtr> &imm_dom_map) {
    const auto &blocks = func->get_blocks();
    if (blocks.empty()) return;
    const BlockPtr entry_block = blocks.front();
    for (const auto &block: blocks) {
        if (block == entry_block) continue; // 入口块没有直接支配者
        const auto &dominators = dominator.at(block);
        std::unordered_set<BlockPtr> strict_dominators;
        for (const auto &d: dominators) {
            if (d != block) {
                strict_dominators.insert(d);
            }
        }
        BlockPtr idom = nullptr;
        for (const auto &d_candidate: strict_dominators) {
            bool valid = true;
            for (const auto &d_prime: strict_dominators) {
                if (d_prime == d_candidate) continue;
                if (const auto &dom_of_candidate = dominator.at(d_candidate);
                    !dom_of_candidate.count(d_prime)) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                idom = d_candidate;
                break;
            }
        }
        if (idom) {
            imm_dom_map[block] = idom;
        } else {
            log_error("No immediate dominator found for block %s", block->get_name().c_str());
        }
    }
}

// Lengauer–Tarjan 算法求解有向图的支配树
// 参见：https://oi-wiki.org/graph/dominator-tree/
struct LengauerTarjan {
    std::vector<BlockPtr> dfs_order;
    std::unordered_map<BlockPtr, size_t> dfs_num;
    std::unordered_map<BlockPtr, BlockPtr> parent, ancestor, semi, idom;
    std::unordered_map<BlockPtr, BlockPtr> best;
    std::unordered_map<BlockPtr, std::vector<BlockPtr>> bucket;
    const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &succ_map;

    explicit LengauerTarjan(const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &succ_map)
        : succ_map(succ_map) {}

    void dfs(const BlockPtr &v) {
        dfs_num[v] = dfs_order.size();
        dfs_order.push_back(v);
        if (const auto it = succ_map.find(v); it != succ_map.end()) {
            for (const auto &w: it->second) {
                if (!dfs_num.count(w)) {
                    parent[w] = v;
                    dfs(w);
                }
            }
        }
    }

    BlockPtr find(BlockPtr v) {
        if (!ancestor.count(v) || ancestor[v] == nullptr) return v;
        compress(v);
        return best[v];
    }

    void compress(const BlockPtr &v) {
        if (ancestor.count(v) && ancestor[v] != nullptr &&
            ancestor.count(ancestor[v]) && ancestor[ancestor[v]] != nullptr) {
            compress(ancestor[v]);
            if (dfs_num[semi[best[ancestor[v]]]] < dfs_num[semi[best[v]]]) {
                best[v] = best[ancestor[v]];
            }
            ancestor[v] = ancestor[ancestor[v]];
        }
    }

    void compute(const FunctionPtr &func) {
        const BlockPtr entry = func->get_blocks().front();
        dfs_num.clear();
        dfs_order.clear();
        parent.clear();
        ancestor.clear();
        semi.clear();
        idom.clear();
        best.clear();
        bucket.clear();

        parent[entry] = nullptr;
        dfs(entry);

        for (const auto &v: dfs_order) {
            ancestor[v] = nullptr;
            best[v] = v;
            semi[v] = v;
        }

        for (auto it = dfs_order.rbegin(); it != dfs_order.rend(); ++it) {
            const BlockPtr &v = *it;
            if (v == entry) {
                continue;
            }
            for (const auto &pred: func->get_blocks()) {
                if (succ_map.count(pred) && succ_map.at(pred).count(v)) {
                    BlockPtr u = pred;
                    BlockPtr s = u;
                    if (dfs_num[u] > dfs_num[v]) {
                        s = semi[find(u)];
                    }
                    if (dfs_num[s] < dfs_num[semi[v]]) {
                        semi[v] = s;
                    }
                }
            }
            bucket[semi[v]].push_back(v);
            ancestor[v] = parent[v];
            if (parent[v] != nullptr) {
                for (const auto &w: bucket[parent[v]]) {
                    BlockPtr u = find(w);
                    idom[w] = semi[u] == semi[w] ? parent[v] : u;
                }
                bucket[parent[v]].clear();
            }
        }
        for (const auto &v: dfs_order) {
            if (v != entry && idom[v] != semi[v]) {
                idom[v] = idom[idom[v]];
            }
        }
        idom[entry] = nullptr;
    }
};

// 构建支配子树中的直接子节点映射
void build_dominance_children(const FunctionPtr &func,
                              const std::unordered_map<BlockPtr, BlockPtr> &imm_dom_map,
                              std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominance_children_map) {
    dominance_children_map.clear();
    for (const auto &block: func->get_blocks()) {
        dominance_children_map[block];
    }
    for (const auto &[child, idom]: imm_dom_map) {
        dominance_children_map[idom].insert(child);
    }
    std::ostringstream oss;
    oss << "\n▷▷ Dominance children for function: ["
            << func->get_name() << "]\n";
    for (const auto &block: func->get_blocks()) {
        const auto &children = dominance_children_map[block];
        oss << "  ■ Block: \"" << block->get_name() << "\"\n"
                << "    └─ Children: " << format_blocks(children) << "\n";
    }
    log_debug("%s", oss.str().c_str());
}

// 构建支配边界
void build_dominance_frontier(const FunctionPtr &func,
                              const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &pred_map,
                              const std::unordered_map<BlockPtr, BlockPtr> &imm_dom_map,
                              std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominance_frontier) {
    dominance_frontier.clear();
    const BlockPtr entry_block = func->get_blocks().front();
    for (const auto &x_block: func->get_blocks()) {
        const auto &x_preds = pred_map.at(x_block);
        for (const auto &p: x_preds) {
            // 遍历每个前驱块
            BlockPtr runner = p;
            BlockPtr x_idom = nullptr;
            // 获取X块的直接支配者
            if (x_block == entry_block) {
                x_idom = entry_block; // 入口块支配自身
            } else {
                const auto it = imm_dom_map.find(x_block);
                if (it == imm_dom_map.end()) {
                    log_error("Block %s has no immediate dominator", x_block->get_name().c_str());
                }
                x_idom = it->second;
            }
            // 沿支配链向上传播支配边界
            while (runner != x_idom) {
                dominance_frontier[runner].insert(x_block);
                // 处理入口块的边界情况
                if (runner == entry_block) break; // 入口无法继续回溯
                // 获取当前runner的直接支配者
                const auto runner_it = imm_dom_map.find(runner);
                if (runner_it == imm_dom_map.end()) {
                    log_error("Block %s has no immediate dominator", runner->get_name().c_str());
                }
                runner = runner_it->second; // 向上回溯
            }
        }
    }
    std::ostringstream oss;
    oss << "\n▷▷ Dominance frontier for function: ["
            << func->get_name() << "]\n";
    for (const auto &block: func->get_blocks()) {
        const auto &frontier = dominance_frontier[block];
        oss << "  ■ Block: \"" << block->get_name() << "\"\n"
                << "    └─ Frontier: " << format_blocks(frontier) << "\n";
    }
    log_debug("%s", oss.str().c_str());
}

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
}

void Pass::ControlFlowGraph::analyze(const std::shared_ptr<const Mir::Module> module) {
    predecessors_.clear();
    successors_.clear();
    dominator_blocks_.clear();
    dominated_blocks_.clear();
    immediate_dominator_.clear();
    dominance_children_.clear();
    dominance_frontier_.clear();
    post_order_blocks_.clear();
    for (const auto &func: *module) {
        auto &pred_map = predecessors_[func], // 前驱块
             &succ_map = successors_[func], // 后驱块
             &dominator_map = dominator_blocks_[func], // 支配该块的所有块集合（含自身）
             &dominated_map = dominated_blocks_[func]; // 被该块支配的所有块集合（含自身）
        auto &imm_dom_map = immediate_dominator_[func]; // 该块的唯一直接支配者（支配树中的父节点）
        auto &dominance_children_map = dominance_children_[func], // 该块在支配树中的直接子节点
             &dominance_frontier_map = dominance_frontier_[func]; // 该块的支配边界
        auto &post_order_blocks = post_order_blocks_[func]; // func中所有block的后序遍历
        build_predecessors_successors(func, pred_map, succ_map);
        build_dominators_dominated(func, pred_map, dominator_map, dominated_map);
        // build_immediate_dominators(func, dominator_map, imm_dom_map);
        LengauerTarjan lt(succ_map);
        lt.compute(func);
        for (const auto &block: func->get_blocks()) {
            imm_dom_map[block] = lt.idom[block];
        }
        imm_dom_map.erase(func->get_blocks().front());
        build_dominance_children(func, imm_dom_map, dominance_children_map);
        build_dominance_frontier(func, pred_map, imm_dom_map, dominance_frontier_map);
        build_post_order(func, dominance_children_map, post_order_blocks);
    }
}

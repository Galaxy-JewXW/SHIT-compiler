#include <numeric>

#include "Mir/Instruction.h"
#include "Pass/Analysis.h"

using FunctionPtr = std::shared_ptr<Mir::Function>;
using BlockPtr = std::shared_ptr<Mir::Block>;

// 输出基本块集合的辅助方法
static std::string format_blocks(const std::unordered_set<BlockPtr> &blocks) {
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
static void build_predecessors_successors(const FunctionPtr &func,
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
static void build_dominators_dominated(const FunctionPtr &func,
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

static void build_immediate_dominators(const FunctionPtr &func,
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

// 构建支配子树中的直接子节点映射
static void build_dominance_children(const FunctionPtr &func,
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
static void build_dominance_frontier(const FunctionPtr &func,
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

void Pass::ControlFlowGraph::analyze(const std::shared_ptr<const Mir::Module> module) {
    predecessors_.clear();
    successors_.clear();
    dominator_blocks_.clear();
    dominated_blocks_.clear();
    immediate_dominator_.clear();
    dominance_children_.clear();
    dominance_frontier_.clear();
    for (const auto &func: *module) {
        auto &pred_map = predecessors_[func], // 前驱块
             &succ_map = successors_[func], // 后驱块
             &dominator_map = dominator_blocks_[func], // 支配该块的所有块集合（含自身）
             &dominated_map = dominated_blocks_[func]; // 被该块支配的所有块集合（含自身）
        auto &imm_dom_map = immediate_dominator_[func]; // 该块的唯一直接支配者（支配树中的父节点）
        auto &dominance_children_map = dominance_children_[func], // 该块在支配树中的直接子节点
             &dominance_frontier_map = dominance_frontier_[func]; // 该块的支配边界
        build_predecessors_successors(func, pred_map, succ_map);
        build_dominators_dominated(func, pred_map, dominator_map, dominated_map);
        build_immediate_dominators(func, dominator_map, imm_dom_map);
        build_dominance_children(func, imm_dom_map, dominance_children_map);
        build_dominance_frontier(func, pred_map, imm_dom_map, dominance_frontier_map);
    }
}

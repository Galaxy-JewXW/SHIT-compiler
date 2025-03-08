#include "Mir/Instruction.h"
#include "Pass/Analysis.h"

using FunctionPtr = std::shared_ptr<Mir::Function>;
using BlockPtr = std::shared_ptr<Mir::Block>;

namespace Pass {
void LoopAnalysis::analyze(std::shared_ptr<const Mir::Module> module) {
    this->loops_.clear();
    std::shared_ptr<ControlFlowGraph> cfg_info = Pass::create<ControlFlowGraph>();
    std::shared_ptr<Mir::Module> mutable_module = std::const_pointer_cast<Mir::Module>(module);
    cfg_info->run_on(mutable_module);

    for (const auto &func: *module) {
        auto block_predecessors = cfg_info->predecessors(func);
        auto block_successors = cfg_info->successors(func);
        auto block_dominators = cfg_info->dominator(func);

        std::vector<std::shared_ptr<Mir::Block>> headers;
        for (const auto &block: cfg_info->post_order_blocks(func)) {
            for (const auto &successor: block_successors[block]) {
                if (block_dominators[block].find(successor) != block_dominators[block].end()) {
                    headers.push_back(successor);
                }
            }
        } // 每条回边对应一个头节点，每个头节点对应一个循环
          // 按后续遍历节点，使得子循环一定先于父循环被发现

        for (const auto &header_block: headers) {
            std::vector<BlockPtr> latching_blocks;
            for (const auto &predecessor: block_predecessors[header_block]) {
                if (block_dominators[predecessor].find(header_block) != block_dominators[predecessor].end()) {
                    latching_blocks.push_back(predecessor);
                }
            } //确认 latching_blocks,接下来 latching 节点的支配节点组成该循环

            std::vector<BlockPtr> working_set;
            std::vector<BlockPtr> loop_blocks;
            std::vector<BlockPtr> exiting_blocks;
            std::vector<BlockPtr> exit_block;
            std::vector<std::shared_ptr<LoopNodeTreeNode>> child_loop_node;
            for (const auto &latching_block: latching_blocks) {
                working_set.push_back(latching_block);
                loop_blocks.push_back(latching_block);
            } //将 latch 节点加入循环，并使用工作集算法，寻找其支配结点，直至 header
            while (!working_set.empty()) {
                auto &current_block = working_set.back();
                working_set.pop_back();
                if (current_block != header_block) {
                    for (const auto &predecessor: block_predecessors[current_block]) {
                        if (std::find(loop_blocks.begin(), loop_blocks.end(), predecessor) == loop_blocks.end()) {
                            working_set.push_back(predecessor);
                            loop_blocks.push_back(predecessor);
                        }
                    }
                }
                if (auto sub_loop_node = find_block_in_forest(func, current_block)) {
                    auto ancestor = sub_loop_node->get_ancestor();
                    child_loop_node.push_back(ancestor);
                    loop_forest_[func].erase(std::remove(loop_forest_[func].begin(), loop_forest_[func].end(),
                                                         ancestor), loop_forest_[func].end());
                }
            } //将结点的所有前驱结点加入循环结点集（自然循环中，循环所有结点被 header 支配）

            for (auto &block : loop_blocks) {
                for (auto &succ : block_successors[block]) {
                    if (std::find(loop_blocks.begin(), loop_blocks.end(), succ) == loop_blocks.end()) {
                        exiting_blocks.push_back(block);
                        exit_block.push_back(succ);
                    }
                }
            }

            auto new_loop = std::make_shared<Loop>(header_block,loop_blocks,latching_blocks,exiting_blocks,exit_block);
            auto new_loop_node = std::make_shared<LoopNodeTreeNode>(new_loop);
            loops_[func].push_back(new_loop);
            loop_forest_[func].push_back(new_loop_node);

            for (auto &child : child_loop_node) {
                child->set_parent(new_loop_node);
                new_loop_node->add_child(child);
            }
        }

        std::ostringstream oss;
        oss << "\n▷▷ loops in func: ["
                << func->get_name() << "]\n";
        for (const auto &loop: loops_[func]) {
            oss << "  ■ header: \"" << loop->get_header()->get_name() << "\"\n";
            for (const auto &block: loop->get_blocks()) {
                oss << "    block: \"" << block->get_name() << "\"\n";
            }
            for (const auto &block: loop->get_latch_blocks()) {
                oss << "    latch: \"" << block->get_name() << "\"\n";
            }
        }
        log_debug("%s", oss.str().c_str());
    }
}

std::shared_ptr<LoopNodeTreeNode> LoopAnalysis::find_loop_in_forest(const FunctionPtr &func, const std::shared_ptr<Loop>& loop) {
    for (const auto &top_node: loop_forest_[func]) {
        if (auto loop_node = top_node->find_loop(loop)) {
            return loop_node;
        }
    }
    return nullptr;
}

std::shared_ptr<LoopNodeTreeNode> LoopNodeTreeNode::find_loop(const std::shared_ptr<Loop> &loop) {
    if (loop_ == loop) {
        return shared_from_this();
    }
    for (const auto &child: children_) {
        if (auto loop_node = child->find_loop(loop)) {
            return loop_node;
        }
    }
    return nullptr;
}

std::shared_ptr<LoopNodeTreeNode> LoopAnalysis::find_block_in_forest(const FunctionPtr &func, const std::shared_ptr<Mir::Block>& block) {
    for (const auto &top_node: loop_forest_[func]) {
        if (auto loop_node = top_node->find_block_in_loop(block)) {
            return loop_node;
        }
    }
    return nullptr;
}

std::shared_ptr<LoopNodeTreeNode> LoopNodeTreeNode::find_block_in_loop(const std::shared_ptr<Mir::Block>& block) {
    if (auto sub_block = loop_->find_block(block)) {
        return shared_from_this();
    }
    for (const auto &child: children_) {
        if (auto loop_node = child->find_block_in_loop(block)) {
            return loop_node;
        }
    }
    return nullptr;
}

std::shared_ptr<Mir::Block> Loop::find_block(const std::shared_ptr<Mir::Block>& block) {
    if (std::find(blocks_.begin(), blocks_.end(), block) != blocks_.end()) {
        return block;
    }
    return nullptr;
}

void LoopNodeTreeNode::add_block4ancestors(const std::shared_ptr<Mir::Block>& block) {
    this->loop_->add_block(block);
    if (this->get_parent() != nullptr) this->get_parent()->add_block4ancestors(block);
}

}



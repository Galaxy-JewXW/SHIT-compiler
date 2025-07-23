#include "Pass/Analyses/LoopAnalysis.h"
#include "Mir/Instruction.h"
#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"

using FunctionPtr = std::shared_ptr<Mir::Function>;
using BlockPtr = std::shared_ptr<Mir::Block>;

namespace Pass {
void LoopAnalysis::analyze(std::shared_ptr<const Mir::Module> module) {
    std::shared_ptr<ControlFlowGraph> cfg_info = get_analysis_result<ControlFlowGraph>(module);
    const auto dom_info = get_analysis_result<DominanceGraph>(module);
    std::shared_ptr<Mir::Module> mutable_module = std::const_pointer_cast<Mir::Module>(module);
    cfg_info->run_on(mutable_module);

    if (const auto func_size = module->get_functions().size();
        func_size != dirty_funcs_.size() || func_size != loop_forest_.size() || func_size != loops_.size()) {
        loops_.clear();
        loop_forest_.clear();
        dirty_funcs_.clear();
    }
    for (const auto &func: *module) {
        dirty_funcs_.try_emplace(func, true);
    }

    // TODO: 这里的逻辑也稍有混乱了，好好设计整理一下
    for (const auto &func: *module) {
        if (!dirty_funcs_.at(func))
            continue;

        auto &block_predecessors = cfg_info->graph(func).predecessors;
        auto &block_successors = cfg_info->graph(func).successors;
        auto &block_dominators = dom_info->graph(func).dominator_blocks;

        std::vector<std::shared_ptr<Mir::Block>> headers;
        for (const auto &block: dom_info->post_order_blocks(func)) {
            for (const auto &predecessor: block_predecessors.at(block)) {
                if (block_dominators.at(predecessor).find(block) != block_dominators.at(predecessor).end()) {
                    headers.push_back(block);
                    break;
                }
            }
        } // 一个节点若有回边则是头节点，每个头节点对应一个循环
        // 按后续遍历节点，使得子循环一定先于父循环被发现

        for (const auto &header_block: headers) {
            std::vector<BlockPtr> latching_blocks;
            for (const auto &predecessor: block_predecessors.at(header_block)) {
                if (block_dominators.at(predecessor).find(header_block) != block_dominators.at(predecessor).end()) {
                    latching_blocks.push_back(predecessor);
                }
            } // 确认 latching_blocks,接下来 latching 节点的支配节点组成该循环

            std::vector<BlockPtr> working_set;
            std::vector<BlockPtr> visited_blocks;
            std::vector<BlockPtr> loop_blocks;
            std::vector<BlockPtr> exiting_blocks;
            std::vector<BlockPtr> exit_block;
            std::vector<std::shared_ptr<LoopNodeTreeNode>> child_loop_node;
            for (const auto &latching_block: latching_blocks) {
                working_set.push_back(latching_block);
                visited_blocks.push_back(latching_block);
            } // 将 latch 节点加入循环，并使用工作集算法，寻找其支配结点，直至 header
            while (!working_set.empty()) {
                auto current_block = working_set.back();
                working_set.pop_back();
                if (current_block != header_block) {
                    for (const auto &predecessor: block_predecessors.at(current_block)) {
                        if (std::find(visited_blocks.begin(), visited_blocks.end(), predecessor) ==
                            visited_blocks.end()) {
                            working_set.push_back(predecessor);
                            visited_blocks.push_back(predecessor);
                        }
                    }
                }
                if (auto sub_loop_node = find_block_in_forest(func, current_block)) {
                    auto ancestor = sub_loop_node->get_ancestor();
                    child_loop_node.push_back(ancestor);
                    loop_forest_[func].erase(
                            std::remove(loop_forest_[func].begin(), loop_forest_[func].end(), ancestor),
                            loop_forest_[func].end());
                } else
                    loop_blocks.push_back(current_block);
            } // 将结点的所有前驱结点加入循环结点集（自然循环中，循环所有结点被 header 支配）

            for (auto &block: loop_blocks) {
                for (auto &succ: block_successors.at(block)) {
                    if (std::find(loop_blocks.begin(), loop_blocks.end(), succ) == loop_blocks.end()) {
                        exiting_blocks.push_back(block);
                        if (std::find(exit_block.begin(), exit_block.end(), succ) == exit_block.end())
                            exit_block.push_back(succ);
                    }
                }
            }

            auto new_loop =
                    std::make_shared<Loop>(header_block, loop_blocks, latching_blocks, exiting_blocks, exit_block);
            auto new_loop_node = std::make_shared<LoopNodeTreeNode>(new_loop);
            loops_[func].push_back(new_loop);
            loop_forest_[func].push_back(new_loop_node);

            for (auto &child: child_loop_node) {
                child->set_parent(new_loop_node);
                new_loop_node->add_child(child);
            }
        }

        std::ostringstream oss;
        oss << "\n▷▷ loops in func: [" << func->get_name() << "]\n";
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

std::shared_ptr<LoopNodeTreeNode> LoopAnalysis::find_loop_in_forest(const FunctionPtr &func,
                                                                    const std::shared_ptr<Loop> &loop) {
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

std::shared_ptr<LoopNodeTreeNode> LoopAnalysis::find_block_in_forest(const FunctionPtr &func,
                                                                     const std::shared_ptr<Mir::Block> &block) {
    for (const auto &top_node: loop_forest_[func]) {
        if (auto loop_node = top_node->find_block_in_loop(block)) {
            return loop_node;
        }
    }
    return nullptr;
}

bool LoopAnalysis::is_dirty() const {
    return std::any_of(dirty_funcs_.begin(), dirty_funcs_.end(), [](const auto &pair) { return pair.second; });
}

bool LoopAnalysis::is_dirty(const std::shared_ptr<Mir::Function> &function) const { return dirty_funcs_.at(function); }


void LoopAnalysis::set_dirty(const FunctionPtr &func) {
    if (dirty_funcs_[func]) {
        return;
    }
    dirty_funcs_[func] = true;
}

std::shared_ptr<LoopNodeTreeNode> LoopNodeTreeNode::find_block_in_loop(const std::shared_ptr<Mir::Block> &block) {
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

std::shared_ptr<Mir::Block> Loop::find_block(const std::shared_ptr<Mir::Block> &block) {
    if (std::find(blocks_.begin(), blocks_.end(), block) != blocks_.end()) {
        return block;
    }
    return nullptr;
}

bool Loop::contain_block(const std::shared_ptr<Mir::Block> &block) {
    if (find_block(block))
        return true;
    return false;
}


void LoopNodeTreeNode::add_block4ancestors(const std::shared_ptr<Mir::Block> &block) {
    this->loop_->add_block(block);
    if (this->get_parent() != nullptr)
        this->get_parent()->add_block4ancestors(block);
}

bool LoopNodeTreeNode::def_value(const std::shared_ptr<Mir::Value> &value) {

    if (value->is<Mir::Const>() || value->is<Mir::Argument>())
        return false;

    auto cond_instr = value->as<Mir::Instruction>();
    auto block = cond_instr->get_block();
    if (this->get_children().empty()) {
        auto loop = this->get_loop();
        if (loop->contain_block(block))
            return true;
    }
    for (const auto &child_node: this->get_children()) {
        if (child_node->def_value(value))
            return true;
    }

    return false;
}

std::shared_ptr<LoopNodeClone> LoopNodeTreeNode::clone_loop_node() {
    auto clone_info = std::make_shared<LoopNodeClone>();
    clone_info->node_src = shared_from_this();
    clone_info->node_cpy = std::make_shared<LoopNodeTreeNode>(std::make_shared<Loop>());
    for (const auto &child_node: this->get_children()) {
        auto child_clone = child_node->clone_loop_node();
        clone_info->node_cpy->add_child(child_clone->node_cpy);
        clone_info->merge(child_clone);
    }
    for (const auto &block: this->get_loop()->get_blocks()) {
        auto new_block = block->cloneinfo_to_func(clone_info, block->get_function());
        clone_info->node_cpy->get_loop()->add_block(new_block);
    }
    clone_info->node_cpy->fix_clone_info(clone_info);

    clone_info->node_cpy->get_loop()->set_preheader(
            clone_info->get_value_reflect(this->get_loop()->get_preheader())->as<Mir::Block>());
    clone_info->node_cpy->get_loop()->set_header(
            clone_info->get_value_reflect(this->get_loop()->get_header())->as<Mir::Block>());
    clone_info->node_cpy->get_loop()->set_latch(
            clone_info->get_value_reflect(this->get_loop()->get_latch())->as<Mir::Block>());
    for (const auto &block: this->get_loop()->get_latch_blocks()) {
        auto new_block = clone_info->get_value_reflect(block)->as<Mir::Block>();
        clone_info->node_cpy->get_loop()->add_latch_block(new_block);
    }
    for (const auto &block: this->get_loop()->get_exitings()) {
        auto new_block = clone_info->get_value_reflect(block)->as<Mir::Block>();
        clone_info->node_cpy->get_loop()->add_exitings(new_block);
    }
    for (const auto &block: this->get_loop()->get_exits()) {
        auto new_block = clone_info->get_value_reflect(block)->as<Mir::Block>();
        clone_info->node_cpy->get_loop()->add_exits(new_block);
    }
}

void LoopNodeTreeNode::fix_clone_info(const std::shared_ptr<LoopNodeClone> &clone_info) {
    for (const auto &child_node: this->get_children())
        child_node->fix_clone_info(clone_info);

    for (const auto &block: this->get_loop()->get_blocks())
        block->fix_clone_info(clone_info);
}

int LoopAnalysis::get_block_depth(const FunctionPtr &func, const std::shared_ptr<Mir::Block> &block) {
    auto loop_node = find_block_in_forest(func, block);
    if (nullptr == loop_node)
        return 0;

    int depth = 0;
    while (loop_node != nullptr) {
        depth++;
        loop_node = loop_node->get_parent();
    }
    return depth;
}

void LoopNodeClone::merge(const std::shared_ptr<LoopNodeClone> &clone_info) {
    for (const auto &pair: clone_info->get_value_map()) {
        if (this->get_value_map().find(pair.first) != this->get_value_map().end()) {
            log_error("Loop node clone error!");
        }
    }
    this->get_value_map().insert(clone_info->get_value_map().begin(), clone_info->get_value_map().end());
}
} // namespace Pass

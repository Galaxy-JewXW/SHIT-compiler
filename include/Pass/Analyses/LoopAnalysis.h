#ifndef LOOPANALYSIS_H
#define LOOPANALYSIS_H

#include "Pass/Analysis.h"

namespace Pass {
class Loop {
    using FunctionPtr = std::shared_ptr<Mir::Function>;
    using BlockPtr = std::shared_ptr<Mir::Block>;
    BlockPtr header_;
    BlockPtr preheader_;
    BlockPtr latch_;
    std::vector<BlockPtr> blocks_;
    std::vector<BlockPtr> latch_blocks_;
    std::vector<BlockPtr> exitings_;
    std::vector<BlockPtr> exits_;

public:
    Loop(BlockPtr header, const std::vector<BlockPtr> &blocks, const std::vector<BlockPtr> &latch_blocks,
         const std::vector<BlockPtr> &exitings, const std::vector<BlockPtr> &exits) :
        header_{std::move(header)}, blocks_{blocks}, latch_blocks_{latch_blocks}, exitings_{exitings}, exits_{exits} {}

    [[nodiscard]] BlockPtr get_header() const { return header_; }
    [[nodiscard]] BlockPtr get_preheader() const { return preheader_; }
    [[nodiscard]] BlockPtr get_latch() const { return latch_; }
    [[nodiscard]] std::vector<BlockPtr> &get_blocks() { return blocks_; }
    [[nodiscard]] std::vector<BlockPtr> &get_latch_blocks() { return latch_blocks_; }
    [[nodiscard]] std::vector<BlockPtr> &get_exitings() { return exitings_; }
    [[nodiscard]] std::vector<BlockPtr> &get_exits() { return exits_; }

    std::shared_ptr<Mir::Block> find_block(const std::shared_ptr<Mir::Block> &block);

    bool contain_block(const std::shared_ptr<Mir::Block> &block);

    void set_preheader(BlockPtr preheader) { preheader_ = std::move(preheader); }
    void set_latch(BlockPtr latch) { latch_ = std::move(latch); }

    void add_block(const std::shared_ptr<Mir::Block> &block) { blocks_.push_back(block); }
};

class LoopNodeTreeNode : public std::enable_shared_from_this<LoopNodeTreeNode> {
public:
    explicit LoopNodeTreeNode(std::shared_ptr<Loop> loop) : loop_{std::move(loop)} {}

    void add_child(std::shared_ptr<LoopNodeTreeNode> child) { children_.push_back(std::move(child)); }

    void remove_child(const std::shared_ptr<LoopNodeTreeNode> &child) {
        if (const auto it = std::find(children_.begin(), children_.end(), child); it != children_.end()) {
            children_.erase(it);
        }
    }

    void set_parent(std::shared_ptr<LoopNodeTreeNode> parent) { parent_ = std::move(parent); }

    std::vector<std::shared_ptr<LoopNodeTreeNode>> &get_children() { return children_; }

    std::shared_ptr<LoopNodeTreeNode> get_parent() { return parent_; }

    std::shared_ptr<LoopNodeTreeNode> get_ancestor() {
        if (this->get_parent() == nullptr) {
            return shared_from_this();
        } else {
            return this->get_parent()->get_ancestor();
        }
    }

    std::shared_ptr<Loop> get_loop() { return loop_; }

    void add_block4ancestors(const std::shared_ptr<Mir::Block> &block);

    std::shared_ptr<LoopNodeTreeNode> find_loop(const std::shared_ptr<Loop> &loop);

    std::shared_ptr<LoopNodeTreeNode> find_block_in_loop(const std::shared_ptr<Mir::Block> &block);

private:
    std::shared_ptr<Loop> loop_;
    std::shared_ptr<LoopNodeTreeNode> parent_;
    std::vector<std::shared_ptr<LoopNodeTreeNode>> children_;
};


class LoopAnalysis final : public Analysis {
public:
    using FunctionPtr = std::shared_ptr<Mir::Function>;
    using BlockPtr = std::shared_ptr<Mir::Block>;

    explicit LoopAnalysis() : Analysis("LoopAnalysis") {}

    void analyze(std::shared_ptr<const Mir::Module> module) override;

    const std::vector<std::shared_ptr<Loop>> &loops(const FunctionPtr &func) const {
        const auto it = loops_.find(func);
        if (it == loops_.end()) {
            log_error("Function not existed: %s", func->get_name().c_str());
        }
        return it->second;
    }

    const std::vector<std::shared_ptr<LoopNodeTreeNode>> &loop_forest(const FunctionPtr &func) const {
        const auto it = loop_forest_.find(func);
        if (it == loop_forest_.end()) {
            static const std::vector<std::shared_ptr<LoopNodeTreeNode>> empty_vector;
            return empty_vector;
        }
        return it->second;
    }

    std::shared_ptr<LoopNodeTreeNode> find_loop_in_forest(const FunctionPtr &func, const std::shared_ptr<Loop> &loop);

    int get_block_depth(const FunctionPtr &func, const std::shared_ptr<Mir::Block> &block);

    std::shared_ptr<LoopNodeTreeNode> find_block_in_forest(const FunctionPtr &func,
                                                           const std::shared_ptr<Mir::Block> &block);

private:
    using FunctLoopsMap = std::unordered_map<std::shared_ptr<Mir::Function>, std::vector<std::shared_ptr<Loop>>>;
    FunctLoopsMap loops_;
    using FunctLoopForestMap =
            std::unordered_map<std::shared_ptr<Mir::Function>, std::vector<std::shared_ptr<LoopNodeTreeNode>>>;
    FunctLoopForestMap loop_forest_;
};
} // namespace Pass

#endif // LOOPANALYSIS_H

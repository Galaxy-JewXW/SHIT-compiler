#ifndef DOMINANCEGRAPH_H
#define DOMINANCEGRAPH_H

#include "Pass/Analysis.h"

namespace Pass {
// DominanceGraph 构建基本块的支配图
class DominanceGraph final : public Analysis {
public:
    using FunctionPtr = std::shared_ptr<Mir::Function>;
    using BlockPtr = std::shared_ptr<Mir::Block>;
    using BlockPtrMap = std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>>;

    explicit DominanceGraph() : Analysis("DominanceGraph") {}

    struct Graph {
        // 被支配块集合：{ block -> {被该块支配的所有块集合（含自身）} }
        BlockPtrMap dominated_blocks{};
        // 支配块集合：{ block -> {支配该块的所有块集合（含自身）} }
        BlockPtrMap dominator_blocks{};
        // 直接支配者：{ block -> 该块的唯一直接支配者（支配树中的父节点） }
        std::unordered_map<BlockPtr, BlockPtr> immediate_dominator{};
        // 支配树子节点：{ block -> {该块在支配树中的直接子节点} }
        BlockPtrMap dominance_children{};
        // 支配边界集合：function -> { block -> {该块的支配边界} }
        BlockPtrMap dominance_frontier{};
    };

    const Graph &graph(const FunctionPtr &func) const {
        const auto it = graphs_.find(func);
        if (it == graphs_.end()) [[unlikely]] {
            log_error("Function not existed: %s", func->get_name().c_str());
        }
        return it->second;
    }

    std::vector<BlockPtr> dom_tree_layer(const FunctionPtr &func);

    std::vector<BlockPtr> post_order_blocks(const FunctionPtr &func);

    bool is_dirty() const override;

    void set_dirty(const FunctionPtr &func);

    void remove(const FunctionPtr &func) {
        graphs_.erase(func);
    }

protected:
    void analyze(std::shared_ptr<const Mir::Module> module) override;

private:
    std::unordered_map<FunctionPtr, Graph> graphs_;

    std::unordered_map<FunctionPtr, bool> dirty_funcs_;
};
}

#endif //DOMINANCEGRAPH_H

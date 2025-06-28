#ifndef CONTROLFLOWGRAPH_H
#define CONTROLFLOWGRAPH_H

#include <unordered_set>

#include "Pass/Analysis.h"

namespace Pass {
// ControlFlowGraph 构建控制流图
class ControlFlowGraph final : public Analysis {
public:
    using FunctionPtr = std::shared_ptr<Mir::Function>;
    using BlockPtr = std::shared_ptr<Mir::Block>;
    using BlockPtrMap = std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>>;

    struct Graph {
        // 前驱块关系：{ block -> {所有前驱块} }
        BlockPtrMap predecessors{};
        // 后继块关系：{ block -> {所有后继块} }
        BlockPtrMap successors{};
    };

    const Graph &graph(const FunctionPtr &func) const {
        const auto it = graphs_.find(func);
        if (it == graphs_.end()) [[unlikely]] {
            log_error("Function not existed: %s", func->get_name().c_str());
        }
        return it->second;
    }

    explicit ControlFlowGraph() : Analysis("ControlFlowGraph") {}

    bool is_dirty() const override;

    bool is_dirty(const std::shared_ptr<Mir::Function> &function) const override;

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

#endif //CONTROLFLOWGRAPH_H

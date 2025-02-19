#ifndef ANALYSIS_H
#define ANALYSIS_H
#include <unordered_set>

#include "Pass.h"

namespace Pass {
// 计算相关IR单元的高层信息，但不对其进行修改
// 提供其它pass需要查询的信息并提供查询接口
class Analysis : public Pass {
public:
    explicit Analysis(const std::string &name) : Pass(PassType::ANALYSIS, name) {}

    void run_on(const std::shared_ptr<Mir::Module> module) override {
        // 强制转换为const版本保证只读访问
        const auto const_module = std::const_pointer_cast<const Mir::Module>(module);
        analyze(const_module);
    }

protected:
    // 子类必须实现的纯虚函数（只读版本）
    virtual void analyze(std::shared_ptr<const Mir::Module> module) = 0;
};

// ControlFlowGraph构建控制流图
// 每个Function对应一套独立的CFG信息，键为FunctionPtr，代表不同的函数
class ControlFlowGraph final : public Analysis {
public:
    using FunctionPtr = std::shared_ptr<Mir::Function>;
    using BlockPtr = std::shared_ptr<Mir::Block>;
    explicit ControlFlowGraph() : Analysis("ControlFlowGraph") {}

    const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &predecessors(const FunctionPtr &func) const {
        const auto it = predecessors_.find(func);
        if (it == predecessors_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &successors(const FunctionPtr &func) const {
        const auto it = successors_.find(func);
        if (it == successors_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominated(const FunctionPtr &func) const {
        const auto it = dominated_blocks_.find(func);
        if (it == dominated_blocks_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominator(const FunctionPtr &func) const {
        const auto it = dominator_blocks_.find(func);
        if (it == dominator_blocks_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    const std::unordered_map<BlockPtr, BlockPtr> &immediate_dominator(const FunctionPtr &func) const {
        const auto it = immediate_dominator_.find(func);
        if (it == immediate_dominator_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &dominance_children(
        const FunctionPtr &func) const {
        const auto it = dominance_children_.find(func);
        if (it == dominance_children_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    const std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>> &
    dominance_frontier(const FunctionPtr &func) const {
        const auto it = dominance_frontier_.find(func);
        if (it == dominance_frontier_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

protected:
    void analyze(std::shared_ptr<const Mir::Module> module) override;

private:
    using FuncBlockMap = std::unordered_map<FunctionPtr, std::unordered_map<BlockPtr, std::unordered_set<BlockPtr>>>;
    // 前驱块关系：function -> { block -> {所有前驱块} }
    FuncBlockMap predecessors_;

    // 后继块关系：function -> { block -> {所有后继块} }
    FuncBlockMap successors_;

    // 被支配块集合：function -> { block -> {被该块支配的所有块集合（含自身）} }
    FuncBlockMap dominated_blocks_;

    // 支配块集合：function -> { block -> {支配该块的所有块集合（含自身）} }
    FuncBlockMap dominator_blocks_;

    // 直接支配者：function -> { block -> 该块的唯一直接支配者（支配树中的父节点） }
    std::unordered_map<FunctionPtr, std::unordered_map<BlockPtr, BlockPtr>> immediate_dominator_;

    // 支配树子节点：function -> { block -> {该块在支配树中的直接子节点} }
    FuncBlockMap dominance_children_;

    // 支配边界集合：function -> { block -> {该块的支配边界} }
    FuncBlockMap dominance_frontier_;
};

// FunctionAnalysis 函数分析
// 构建函数调用图
// TODO: 分析函数是否有副作用
class FunctionAnalysis final : public Analysis {
public:
    using FunctionPtr = std::shared_ptr<Mir::Function>;
    using FunctionMap = std::unordered_map<FunctionPtr, std::unordered_set<FunctionPtr>>;
    using FunctionSet = std::unordered_set<FunctionPtr>;
    explicit FunctionAnalysis() : Analysis("FunctionCallGraph") {}

    const FunctionSet &call_graph_func(const FunctionPtr &func) const {
        const auto it = call_graph_.find(func);
        if (it == call_graph_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    const FunctionSet &call_graph_reverse_func(const FunctionPtr &func) const {
        const auto it = call_graph_reverse_.find(func);
        if (it == call_graph_reverse_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    const FunctionMap &call_graph() const { return call_graph_; }

    const FunctionMap &call_graph_reverse() const { return call_graph_reverse_; }

protected:
    void analyze(std::shared_ptr<const Mir::Module> module) override;

private:
    // 函数调用图：function -> { function调用的函数集合 }
    FunctionMap call_graph_;
    // 反向函数调用图：function -> { function被调用的函数集合 }
    FunctionMap call_graph_reverse_;
};
}

#endif //ANALYSIS_H

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

/**
 * FunctionAnalysis 函数分析
 * 1. 构建函数调用图
 * 2. 分析函数是否有副作用
 *   - 函数中包含对全局变量的store指令
 *   - 函数中包含对指针实参的gep与store指令
 *   - 函数调用了IO相关的库函数
 */
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

    bool has_side_effect(const FunctionPtr &func) const {
        const auto it = side_effect_functions_.find(func);
        return it != side_effect_functions_.end();
    }

    bool accept_input(const FunctionPtr &func) const {
        const auto it = accept_input_functions_.find(func);
        return it != accept_input_functions_.end();
    }

    bool return_output(const FunctionPtr &func) const {
        const auto it = return_output_functions_.find(func);
        return it != return_output_functions_.end();
    }

protected:
    void analyze(std::shared_ptr<const Mir::Module> module) override;

private:
    // 函数调用图：function -> { function调用的函数集合 }
    FunctionMap call_graph_;
    // 反向函数调用图：function -> { function被调用的函数集合 }
    FunctionMap call_graph_reverse_;
    // 函数是否有副作用：具有副作用的函数集合
    FunctionSet side_effect_functions_;
    // 函数是否接受输入：接受输入的函数集合
    FunctionSet accept_input_functions_;
    // 函数是否返回输出：返回输出的函数集合
    FunctionSet return_output_functions_;

    void clear() {
        call_graph_.clear();
        call_graph_reverse_.clear();
        side_effect_functions_.clear();
        accept_input_functions_.clear();
        return_output_functions_.clear();
    }
};

class LoopAnalysis : public Analysis {
public:
    using FunctionPtr = std::shared_ptr<Mir::Function>;
    using BlockPtr = std::shared_ptr<Mir::Block>;

    struct Loop {
        BlockPtr header;
        BlockPtr preheader;
        BlockPtr latch;
        std::vector<BlockPtr> blocks;
        std::vector<BlockPtr> latch_blocks;
        std::vector<BlockPtr> exitings;
    };

    explicit LoopAnalysis() : Analysis("LoopAnalysis") {}
    void analyze(std::shared_ptr<const Mir::Module> module) override;

private:
    using FunctLoopsMap = std::unordered_map<std::shared_ptr<Mir::Function>, std::vector<std::shared_ptr<Loop>>>;
    FunctLoopsMap loops_;
 };
}

#endif //ANALYSIS_H

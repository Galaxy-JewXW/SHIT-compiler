#ifndef CONTROLFLOW_H
#define CONTROLFLOW_H

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Analyses/FunctionAnalysis.h"
#include "Pass/Transform.h"

namespace Pass {
class BranchProbabilityAnalysis;
}

namespace Pass {
/**
 * 简化控制流：
 * 1. 删除没有前驱块（即无法到达）的基本块
 * 2.
 * 如果某一个基本块只有一个前驱，且前驱的后继只有当前基本块，则将当前基本块与其前驱合并
 * 3. 消除只有一个前驱块的phi节点
 * 4. 消除只包含单个非条件跳转的基本块
 * 5. 消除只包含单个条件跳转的基本块
 */
class SimplifyControlFlow final : public Transform {
public:
    explicit SimplifyControlFlow() : Transform("SimplifyControlFlow") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;

public:
    static bool remove_unreachable_blocks(const std::shared_ptr<Mir::Function> &func);

    static void remove_deleted_blocks(const std::shared_ptr<Mir::Function> &func);

private:
    bool run_on_func(const std::shared_ptr<Mir::Function> &func) const;

    std::shared_ptr<ControlFlowGraph> cfg_info{nullptr};
};

// 重排序函数内部的基本块，减少指令缓存未命中和分支预测开销
class BlockPositioning final : public Transform {
public:
    explicit BlockPositioning() : Transform("BlockPositioning") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;

private:
    void run_on_func(const std::shared_ptr<Mir::Function> &func) const;

    std::shared_ptr<ControlFlowGraph> cfg_info{nullptr};

    std::shared_ptr<BranchProbabilityAnalysis> branch_prob_info{nullptr};
};

// 合并嵌套的分支，减少控制流复杂度
class BranchMerging final : public Transform {
public:
    explicit BranchMerging() : Transform("BranchMerging") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;

    void run_on_func(const std::shared_ptr<Mir::Function> &func);

private:
    std::shared_ptr<ControlFlowGraph> cfg_info{nullptr};
    std::shared_ptr<DominanceGraph> dom_info{nullptr};
};

// 将 if-elif-else 链转换为 switch
class IfChainToSwitch final : public Transform {
public:
    explicit IfChainToSwitch() : Transform("IfChainToSwitch") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;

    void run_on_func(const std::shared_ptr<Mir::Function> &func) const;

private:
    std::shared_ptr<ControlFlowGraph> cfg_info{nullptr};
    std::shared_ptr<DominanceGraph> dom_info{nullptr};
};

// 尾调用优化：将部分 call 指令标记为tail call，消除了函数返回/入栈开销
class TailCallOptimize final : public Transform {
public:
    explicit TailCallOptimize() : Transform("TailCallOptimize") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;

    void run_on_func(const std::shared_ptr<Mir::Function> &func) const;

    void tail_call_detect(const std::shared_ptr<Mir::Function> &func) const;

    void tail_call_eliminate(const std::shared_ptr<Mir::Function> &func) const;

    static bool handle_tail_call(const std::shared_ptr<Mir::Call> &call);

private:
    std::shared_ptr<ControlFlowGraph> cfg_info{nullptr};
    std::shared_ptr<FunctionAnalysis> func_info{nullptr};
};

// 将函数中所有的ret语句汇聚到一个block中，简化cfg
class SingleReturnTransform final : public Transform {
public:
    explicit SingleReturnTransform() : Transform("SingleReturnTransform") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;

private:
    static void run_on_func(const std::shared_ptr<Mir::Function> &func);
};

// 函数内联
class Inlining final : public Transform {
public:
    explicit Inlining() : Transform("Inlining") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    std::shared_ptr<ControlFlowGraph> cfg_info{nullptr};

    std::shared_ptr<FunctionAnalysis> func_info{nullptr};

    [[nodiscard]] bool can_inline(const std::shared_ptr<Mir::Function> &func) const;

    void do_inline(const std::shared_ptr<Mir::Function> &func);

    void replace_call(const std::shared_ptr<Mir::Call> &call, const std::shared_ptr<Mir::Function> &caller,
                      const std::shared_ptr<Mir::Function> &callee) const;
};
} // namespace Pass

#endif

#ifndef CONTROLFLOW_H
#define CONTROLFLOW_H

#include "Pass/Transform.h"
#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Analyses/FunctionAnalysis.h"

namespace Pass {
/**
 * 简化控制流：
 * 1. 删除没有前驱块（即无法到达）的基本块
 * 2. 如果某一个基本块只有一个前驱，且前驱的后继只有当前基本块，则将当前基本块与其前驱合并
 * 3. 消除只有一个前驱块的phi节点
 * 4. 消除只包含单个非条件跳转的基本块
 * 5. 消除只包含单个条件跳转的基本块
 */
class SimplifyControlFlow final : public Transform {
public:
    explicit SimplifyControlFlow() : Transform("SimplifyControlFlow") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void run_on_func(const std::shared_ptr<Mir::Function> &func) const;

public:
    static void remove_unreachable_blocks(const std::shared_ptr<Mir::Function> &func);

    static void remove_deleted_blocks(const std::shared_ptr<Mir::Function> &func);

private:
    std::shared_ptr<ControlFlowGraph> cfg_info;
};

// 重排序函数内部的基本块，减少指令缓存未命中和分支预测开销
class BlockPositioning final : public Transform {
public:
    explicit BlockPositioning() : Transform("BlockPositioning") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void run_on_func(const std::shared_ptr<Mir::Function> &func) const;

private:
    std::shared_ptr<ControlFlowGraph> cfg_info;
};

// 尾递归优化：将尾递归转换为循环
class TailRecursionToLoop final : public Transform {
public:
    explicit TailRecursionToLoop() : Transform("TailRecursionToLoop") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void run_on_func(const std::shared_ptr<Mir::Function> &func) const;

private:
    std::shared_ptr<ControlFlowGraph> cfg_info;
    std::shared_ptr<FunctionAnalysis> func_info;
};

// 合并嵌套的分支，减少控制流复杂度
class BranchMerging final : public Transform {
public:
    explicit BranchMerging() : Transform("BranchMerging") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void run_on_func(const std::shared_ptr<Mir::Function> &func);

private:
    std::shared_ptr<ControlFlowGraph> cfg_info;
    std::shared_ptr<DominanceGraph> dom_info;
};
}

#endif //CONTROLFLOW_H

#ifndef CONTROLFLOW_H
#define CONTROLFLOW_H

#include "Pass/Transform.h"

namespace Pass {
/**
 * 简化控制流：
 * 1. 删除没有前驱块（即无法到达）的基本块
 * 2. 如果某一个基本块只有一个前驱，且前驱的后继只有当前基本块，则将当前基本块与其前驱合并
 * 3. 消除只有一个前驱块的phi节点
 * 4. (弃用)消除只包含单个非条件跳转的基本块
 */
class SimplifyCFG final : public Transform {
public:
    explicit SimplifyCFG() : Transform("SimplifyCFG") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void dfs(const std::shared_ptr<Mir::Block> &current_block);

    void remove_unreachable_blocks_for_phi(const std::shared_ptr<Mir::Phi> &phi,
                                           const std::shared_ptr<Mir::Function> &func) const;

    bool try_merge_blocks(const std::shared_ptr<Mir::Function> &func) const;

    [[deprecated]] bool try_simplify_single_jump(const std::shared_ptr<Mir::Function> &func) const;

    void remove_unreachable_blocks(const std::shared_ptr<Mir::Function> &func);

    bool remove_phi(const std::shared_ptr<Mir::Function> &func) const;

private:
    std::unordered_set<std::shared_ptr<Mir::Block>> visited;

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
}

#endif //CONTROLFLOW_H

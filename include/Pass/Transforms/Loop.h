#ifndef LOOP_H
#define LOOP_H

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Analyses/LoopAnalysis.h"
#include "Pass/Transform.h"

namespace Pass {
class LoopSimplyForm final : public Transform {
public:
    explicit LoopSimplyForm() : Transform("LoopSimplyform") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

class LCSSA final : public Transform {
public:
    explicit LCSSA() : Transform("LCSSA") {}
    void set_cfg(const std::shared_ptr<ControlFlowGraph> &cfg) { cfg_info_ = cfg; }

    void set_dom(const std::shared_ptr<DominanceGraph> &dom) { dom_info_ = dom; }

    void set_loop_info(const std::shared_ptr<LoopAnalysis> &loop_info) { loop_info_ = loop_info; }

    std::shared_ptr<ControlFlowGraph> cfg_info() { return cfg_info_; }

    std::shared_ptr<DominanceGraph> dom_info() { return dom_info_; }

    std::shared_ptr<LoopAnalysis> loop_info() { return loop_info_; }

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void runOnNode(const std::shared_ptr<LoopNodeTreeNode> &loop_node);

    bool usedOutLoop(const std::shared_ptr<Mir::Instruction> &inst, const std::shared_ptr<Loop> &loop);

    void addPhi4Exit(const std::shared_ptr<Mir::Instruction> &inst, const std::shared_ptr<Mir::Block> &exit,
                     const std::shared_ptr<Loop> &loop);

private:
    std::shared_ptr<ControlFlowGraph> cfg_info_;
    std::shared_ptr<DominanceGraph> dom_info_;
    std::shared_ptr<LoopAnalysis> loop_info_;
};

class LoopUnSwitch final : public Transform {
public:
    explicit LoopUnSwitch() : Transform("LoopUnSwitch") {}

protected:
    std::vector<std::shared_ptr<Loop>> un_switched_loops_;

    void transform(std::shared_ptr<Mir::Module> module) override;

    bool un_switching(std::shared_ptr<LoopNodeTreeNode> &loop);

    static void collect_branch(std::shared_ptr<LoopNodeTreeNode> &node, std::vector<std::shared_ptr<Mir::Branch>>& branch_vector);

    void
    handle_branch(std::shared_ptr<LoopNodeTreeNode> &node, std::vector<std::shared_ptr<Mir::Branch>> &branch_vector);
};
class LoopInvariantCodeMotion final : public Transform {
public:
    explicit LoopInvariantCodeMotion() : Transform("LoopInvariantCodeMotion") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};
} // namespace Pass

#endif // LOOP_H

#ifndef LOOP_H
#define LOOP_H

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Analyses/LoopAnalysis.h"
#include "Pass/Transform.h"
#include "Pass/Analyses/SCEVAnalysis.h"

namespace Pass {
class LoopSimplyForm final : public Transform {
public:
    explicit LoopSimplyForm() : Transform("LoopSimplyform") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;
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

    void transform(const std::shared_ptr<Mir::Function> &) override;

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

    static void collect_branch(std::shared_ptr<LoopNodeTreeNode> &node,
                               std::vector<std::shared_ptr<Mir::Branch>> &branch_vector);

    void handle_branch(std::shared_ptr<LoopNodeTreeNode> &node,
                       std::vector<std::shared_ptr<Mir::Branch>> &branch_vector);
};

class InductionVariables final : public Transform {
public:
    explicit InductionVariables() : Transform("InductionVariables") {}

protected:

    std::shared_ptr<SCEVAnalysis> scev_info_;
    std::shared_ptr<LoopAnalysis> loop_info_;
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;
    void run(std::shared_ptr<LoopNodeTreeNode> &loop_node);
    bool get_trip_count(std::shared_ptr<Loop> loop);
    int get_tick_num(std::shared_ptr<SCEVExpr> scev_expr, Mir::Icmp::Op op, int n);
};

class LoopInterchange final : public Transform {
public:
    explicit LoopInterchange() : Transform("LoopInterchange") {}
    void transform(std::shared_ptr<Mir::Module> module) override;
    void run_on(const std::shared_ptr<Mir::Function> &function);
    bool check_on_nest(const std::shared_ptr<LoopNodeTreeNode>& loop_nest);
    void transform_on_nest(std::shared_ptr<LoopNodeTreeNode> loop_nest);

protected:
    std::shared_ptr<LoopAnalysis> loop_info_;
    std::shared_ptr<SCEVAnalysis> scev_info_;
    void get_loops(const std::shared_ptr<LoopNodeTreeNode>& loop_nest, std::vector<std::shared_ptr<LoopNodeTreeNode>> &loops);
    bool is_computable(const std::shared_ptr<LoopNodeTreeNode>& loop_node);
    bool get_dependence_info(std::vector<std::shared_ptr<LoopNodeTreeNode>> &loops);
    int min_nest_depth = 2;
    int max_nest_depth = 10;

    void get_cache_cost_manager(std::vector<std::shared_ptr<LoopNodeTreeNode>> &loops);
};

class ConstLoopUnroll final : public Transform {
public:
    explicit ConstLoopUnroll() : Transform("ConstLoopUnroll") {}

    void transform(std::shared_ptr<Mir::Module> module) override;
    void transform(const std::shared_ptr<Mir::Function> &) override;

    bool try_unroll(std::shared_ptr<LoopNodeTreeNode> loop_node, std::shared_ptr<Mir::Function> func);
protected:
    std::shared_ptr<LoopAnalysis> loop_info_;
    std::shared_ptr<SCEVAnalysis> scev_info_;
    std::shared_ptr<ControlFlowGraph> cfg_info_;
    int max_line_num = 5000;
};

class LoopInvariantCodeMotion final : public Transform {
public:
    explicit LoopInvariantCodeMotion() : Transform("LoopInvariantCodeMotion") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};
} // namespace Pass

#endif // LOOP_H

#ifndef ANALYSIS_H
#define ANALYSIS_H
#include <unordered_set>
#include <utility>

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

    void run_on(const std::shared_ptr<const Mir::Module> &module) {
        analyze(module);
    }

protected:
    // 子类必须实现的纯虚函数（只读版本）
    virtual void analyze(std::shared_ptr<const Mir::Module> module) = 0;
};

template<typename T>
std::shared_ptr<T> get_analysis_result(const std::shared_ptr<Mir::Module> module) {
    static_assert(std::is_base_of_v<Analysis, T>, "T must be a subclass of Analysis");
    const auto analysis = Pass::create<T>();
    analysis->run_on(module);
    return analysis;
}

template<typename T>
std::shared_ptr<T> get_analysis_result(const std::shared_ptr<const Mir::Module> module) {
    static_assert(std::is_base_of_v<Analysis, T>, "T must be a subclass of Analysis");
    const auto analysis = Pass::create<T>();
    analysis->run_on(module);
    return analysis;
}

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

    const std::vector<BlockPtr> &post_order_blocks(const FunctionPtr &func) const {
        const auto it = post_order_blocks_.find(func);
        if (it == post_order_blocks_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    const std::vector<BlockPtr> &dom_tree_layer(const FunctionPtr &func) const {
        const auto it = dom_tree_layer_.find(func);
        if (it == dom_tree_layer_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
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

    // block的后序遍历顺序：function -> { 按照后序遍历排序的block集合 }
    std::unordered_map<FunctionPtr, std::vector<BlockPtr>> post_order_blocks_;

    // block按照支配树上的层顺序：function -> { 按照支配树层顺序排序的block集合 }
    std::unordered_map<FunctionPtr, std::vector<BlockPtr>> dom_tree_layer_;
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

    struct FunctionInfo {
        // 递归调用
        bool is_recursive = false;
        // 叶函数：不做任何调用
        bool is_leaf = false;
        // 对全局内存进行读写
        bool memory_read = false;
        bool memory_write = false;
        bool memory_alloc = false;
        // IO操作
        bool io_read = false;
        bool io_write = false;
        // 返回值
        bool has_return = false;
        // 具有副作用：对传入的指针（数组参数）进行写操作
        bool has_side_effect = false;
        // 无状态依赖：输出与内存无关，即函数执行过程中不存在影响全局内存的行为，且没有副作用
        // 注意无状态依赖并不意味着没有进行IO操作
        bool no_state = false;
    };

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

    FunctionInfo func_info(const FunctionPtr &func) const {
        const auto it = infos_.find(func);
        if (it == infos_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
        return it->second;
    }

    [[nodiscard]] const std::vector<FunctionPtr> &topo() const { return topo_; }

protected:
    void analyze(std::shared_ptr<const Mir::Module> module) override;

private:
    // 函数调用图：function -> { function调用的函数集合 }
    FunctionMap call_graph_;
    // 反向函数调用图：function -> { function被调用的函数集合 }
    FunctionMap call_graph_reverse_;

    std::unordered_map<FunctionPtr, FunctionInfo> infos_;

    std::vector<FunctionPtr> topo_;

    void build_call_graph(const FunctionPtr &func);

    void build_func_attribute(const FunctionPtr &func);

    void transmit_attribute(const std::vector<FunctionPtr> &topo);
};

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
    Loop(BlockPtr header, const std::vector<BlockPtr> &blocks,
         const std::vector<BlockPtr> &latch_blocks, const std::vector<BlockPtr> &exitings,
         const std::vector<BlockPtr> &exits)
        : header_{std::move(header)}, blocks_{blocks}, latch_blocks_{latch_blocks},
          exitings_{exitings}, exits_{exits} {}

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
    explicit LoopNodeTreeNode(std::shared_ptr<Loop> loop): loop_{std::move(loop)} {}

    void add_child(std::shared_ptr<LoopNodeTreeNode> child) {
        children_.push_back(std::move(child));
    }

    void remove_child(const std::shared_ptr<LoopNodeTreeNode> &child) {
        if (const auto it = std::find(children_.begin(), children_.end(), child); it != children_.end()) {
            children_.erase(it);
        }
    }

    void set_parent(std::shared_ptr<LoopNodeTreeNode> parent) {
        parent_ = std::move(parent);
    }

    std::vector<std::shared_ptr<LoopNodeTreeNode>> &get_children() {
        return children_;
    }

    std::shared_ptr<LoopNodeTreeNode> get_parent() {
        return parent_;
    }

    std::shared_ptr<LoopNodeTreeNode> get_ancestor() {
        if (this->get_parent() == nullptr) { return shared_from_this(); } else {
            return this->get_parent()->get_ancestor();
        }
    }

    std::shared_ptr<Loop> get_loop() {
        return loop_;
    }

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
        if (it == loops_.end()) { log_error("Function not existed: %s", func->get_name().c_str()); }
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

private:
    using FunctLoopsMap = std::unordered_map<std::shared_ptr<Mir::Function>, std::vector<std::shared_ptr<Loop>>>;
    FunctLoopsMap loops_;
    using FunctLoopForestMap = std::unordered_map<std::shared_ptr<Mir::Function>, std::vector<std::shared_ptr<
        LoopNodeTreeNode>>>;
    FunctLoopForestMap loop_forest_;

    std::shared_ptr<LoopNodeTreeNode> find_block_in_forest(const FunctionPtr &func,
                                                           const std::shared_ptr<Mir::Block> &block);
};

class AliasAnalysis final : public Analysis {
public:
    struct Result {
        struct PairHasher {
            size_t operator()(const std::pair<size_t, size_t> &p) const {
                return std::hash<size_t>()(p.first) ^ std::hash<size_t>()(p.second) << 1;
            }
        };

        std::unordered_set<std::pair<size_t, size_t>, PairHasher> distinct_pairs;
        std::vector<std::unordered_set<size_t>> distinct_groups;
        std::unordered_map<std::shared_ptr<Mir::Value>, std::vector<size_t>> pointer_attributes;

        void add_pair(const size_t &l, const size_t &r) {
            if (l == r) {
                log_error("Id %lu and %lu cannot be the same", l, r);
            }
            distinct_pairs.insert({l, r});
        }

        void add_value(const std::shared_ptr<Mir::Value> &value, const std::vector<size_t> &attrs) {
            if (!value->get_type()->is_pointer()) {
                log_error("Value %s is not a pointer", value->to_string().c_str());
            }
            pointer_attributes[value] = attrs;
        }
    };

    struct InheritEdge {
        std::shared_ptr<Mir::Value> dst, src1, src2;

        InheritEdge(const std::shared_ptr<Mir::Value> &dst, const std::shared_ptr<Mir::Value> &src1,
                    const std::shared_ptr<Mir::Value> &src2 = nullptr) : dst{dst}, src1{src1}, src2{src2} {}

        bool operator==(const InheritEdge &other) const {
            return dst == other.dst && src1 == other.src1 && src2 == other.src2;
        }

        bool operator==(InheritEdge &&other) const {
            return dst == other.dst && src1 == other.src1 && src2 == other.src2;
        }

        struct Hasher {
            size_t operator()(const InheritEdge &edge) const {
                const size_t h1 = std::hash<std::shared_ptr<Mir::Value>>()(edge.dst),
                             h2 = std::hash<std::shared_ptr<Mir::Value>>()(edge.src1),
                             h3 = std::hash<std::shared_ptr<Mir::Value>>()(edge.src2);
                size_t seed = h1;
                seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                return seed;
            }
        };
    };

    explicit AliasAnalysis() : Analysis("AliasAnalysis") {}

    void run_on_func(const std::shared_ptr<Mir::Function> &func);

    void analyze(std::shared_ptr<const Mir::Module> module) override;

private:
    std::shared_ptr<Mir::Module> module;

    std::shared_ptr<ControlFlowGraph> cfg{nullptr};

    std::vector<std::shared_ptr<Result>> results{};
};
}

#endif //ANALYSIS_H

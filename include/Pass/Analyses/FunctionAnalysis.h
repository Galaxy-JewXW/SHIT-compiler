#ifndef FUNCTIONANALYSIS_H
#define FUNCTIONANALYSIS_H

#include <unordered_set>

#include "Pass/Analysis.h"

namespace Pass {
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
        std::unordered_set<std::shared_ptr<Mir::GlobalVariable>> used_global_variables;
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
}

#endif //FUNCTIONANALYSIS_H

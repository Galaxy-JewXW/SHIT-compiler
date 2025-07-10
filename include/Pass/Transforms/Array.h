#ifndef ARRAY_H
#define ARRAY_H

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Analyses/FunctionAnalysis.h"
#include "Pass/Transform.h"

namespace Pass {
// getelementptr 折叠：将嵌套的 getelementptr 指令链折叠为单一 getelementptr 指令
class GepFolding final : public Transform {
public:
    explicit GepFolding() : Transform("GepFolding") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    std::shared_ptr<DominanceGraph> dom_graph{nullptr};

    void run_on_func(const std::shared_ptr<Mir::Function> &func) const;
};

// 冗余加载消除：跟踪内存的 store 和 load 操作，通过替换重复的 load 来减少不必要的内存访问
class LoadEliminate final : public Transform {
public:
    explicit LoadEliminate() : Transform("LoadEliminate") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    using ValuePtr = std::shared_ptr<Mir::Value>;

    std::shared_ptr<ControlFlowGraph> cfg_info{nullptr};

    std::shared_ptr<DominanceGraph> dom_info{nullptr};

    std::shared_ptr<FunctionAnalysis> function_analysis{nullptr};
    // 待删除的指令列表
    std::unordered_set<std::shared_ptr<Mir::Instruction>> deleted_instructions;
    // 跟踪数组的存储和加载操作
    // 键：基地址(alloca, GlobalVariable, Argument)
    // 值：另一个映射，计入每个索引对应的最新存储值或加载指令
    std::unordered_map<ValuePtr, std::unordered_map<ValuePtr, ValuePtr>> load_indexes, store_indexes;
    // 跟踪全局标量的存储与加载操作
    std::unordered_map<std::shared_ptr<Mir::GlobalVariable>, ValuePtr> load_global, store_global;

    void run_on_func(const std::shared_ptr<Mir::Function> &func);

    void dfs(const std::shared_ptr<Mir::Block> &block);

    void handle_load(const std::shared_ptr<Mir::Load> &load);

    void handle_store(const std::shared_ptr<Mir::Store> &store);

    void handle_call(const std::shared_ptr<Mir::Call> &call);

    void clear() {
        load_indexes.clear();
        store_indexes.clear();
        load_global.clear();
        store_global.clear();
    }
};

// 冗余存储消除：删除同一地址的连续 store 操作与未使用的 store 操作
class StoreEliminate final : public Transform {
public:
    explicit StoreEliminate() : Transform("StoreEliminate") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    using ValuePtr = std::shared_ptr<Mir::Value>;

    std::shared_ptr<FunctionAnalysis> function_analysis{nullptr};

    std::unordered_map<ValuePtr, std::unordered_map<ValuePtr, std::shared_ptr<Mir::Store>>> store_map;

    std::unordered_map<ValuePtr, std::shared_ptr<Mir::Store>> store_global;

    std::unordered_set<std::shared_ptr<Mir::Instruction>> deleted_instructions;

    void run_on_func(const std::shared_ptr<Mir::Function> &func);

    void handle_load(const std::shared_ptr<Mir::Load> &load);

    void handle_store(const std::shared_ptr<Mir::Store> &store);

    void handle_call(const std::shared_ptr<Mir::Call> &call);

    void clear() {
        store_map.clear();
        store_global.clear();
    }
};

// 聚集对象的标量替换: scalar replacement of aggregate
class SROA final : public Transform {
public:
    explicit SROA() : Transform("SROA") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    using IndexMap = std::unordered_map<int, std::vector<std::shared_ptr<Mir::GetElementPtr>>>;

    std::unordered_map<std::shared_ptr<Mir::Alloc>, IndexMap> alloc_index_geps;

    IndexMap index_use;

    std::unordered_set<std::shared_ptr<Mir::Instruction>> deleted_instructions;

    void run_on_func(const std::shared_ptr<Mir::Function> &func);

    bool can_be_split(const std::shared_ptr<Mir::Alloc> &alloc);

    void clear() {
        alloc_index_geps.clear();
        deleted_instructions.clear();
    }
};

// 将全局变量中数组的常量索引访问替换为对应的初始化值
class ConstIndexToValue final : public Transform {
public:
    explicit ConstIndexToValue() : Transform("ConstIndexToValue") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};
} // namespace Pass

#endif // ARRAY_H

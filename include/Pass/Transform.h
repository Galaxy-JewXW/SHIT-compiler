#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "Analysis.h"
#include "Pass.h"
#include "Mir/Instruction.h"

namespace Pass {
// 以某种方式改变和优化IR，并保证改变后的IR仍然合法有效
class Transform : public Pass {
public:
    explicit Transform(const std::string &name) : Pass(PassType::TRANSFORM, name) {}

    void run_on(const std::shared_ptr<Mir::Module> module) override {
        transform(module);
    }

protected:
    virtual void transform(std::shared_ptr<Mir::Module> module) = 0;
};

// 自动地将 alloca 变量提升为寄存器变量，将IR转化为SSA形式
class Mem2Reg final : public Transform {
public:
    explicit Mem2Reg() : Transform("Mem2Reg") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    // 控制流图信息，用于后续基本块支配关系和变量使用/定义分析
    std::shared_ptr<ControlFlowGraph_Old> cfg_info;
    // 当前正在处理的函数对象
    std::shared_ptr<Mir::Function> current_function;
    // 当前被处理的alloca指令，可能被提升为寄存器变量
    std::shared_ptr<Mir::Alloc> current_alloc;
    // 记录当前变量的所有定义指令（生成变量值的指令）
    std::vector<std::shared_ptr<Mir::Instruction>> def_instructions;
    // 记录当前变量的所有使用指令（引用变量值的指令）
    std::vector<std::shared_ptr<Mir::Instruction>> use_instructions;
    // 记录包含当前变量定义的基本块，用于计算Phi节点插入位置
    std::vector<std::shared_ptr<Mir::Block>> def_blocks;
    // 变量重命名时使用的定义栈，栈顶为当前作用域内有效的定义版本
    std::vector<std::shared_ptr<Mir::Value>> def_stack;

    void init_mem2reg();

    void insert_phi();

    void rename_variables(const std::shared_ptr<Mir::Block> &block);
};

class LoopSimplyForm final : public Transform {
public:
    explicit LoopSimplyForm() : Transform("LoopSimplyform") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

class LCSSA final : public Transform {
public:
    explicit LCSSA() : Transform("LCSSA") {}
    void set_cfg(const std::shared_ptr<ControlFlowGraph_Old> &cfg) { cfg_info_ = cfg; }

    std::shared_ptr<ControlFlowGraph_Old> cfg_info() { return cfg_info_; }

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void runOnNode(std::shared_ptr<LoopNodeTreeNode> loop_node);

    bool usedOutLoop(std::shared_ptr<Mir::Instruction> inst, std::shared_ptr<Loop> loop);

    void addPhi4Exit(std::shared_ptr<Mir::Instruction> inst, std::shared_ptr<Mir::Block> exit,
                     std::shared_ptr<Loop> loop);

private:
    std::shared_ptr<ControlFlowGraph_Old> cfg_info_;
};

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

    std::shared_ptr<ControlFlowGraph_Old> cfg_info;
};

// 标准化计算指令 "Binary"
// 为之后的代数变形/GVN做准备
class StandardizeBinary final : public Transform {
public:
    explicit StandardizeBinary() : Transform("StandardizeBinary") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

// 对指令进行代数优化恒等式变形
class AlgebraicSimplify final : public Transform {
public:
    explicit AlgebraicSimplify() : Transform("AlgebraicSimplify") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

// 全局代码移动
// 根据Value之间的依赖关系，将代码的位置重新安排，从而使得一些不必要（不会影响结果）的代码尽可能少执行
class GlobalCodeMotion final : public Transform {
public:
    explicit GlobalCodeMotion() : Transform("GlobalCodeMotion") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    bool is_pinned(const std::shared_ptr<Mir::Instruction> &instruction) const;

    void run_on_func(const std::shared_ptr<Mir::Function> &func);

    void schedule_early(const std::shared_ptr<Mir::Instruction> &instruction);

    void schedule_late(const std::shared_ptr<Mir::Instruction> &instruction);

    int dom_tree_depth(const std::shared_ptr<Mir::Block> &block) const;

    int loop_depth(const std::shared_ptr<Mir::Block> &block) const;

    std::shared_ptr<Mir::Block> find_lca(const std::shared_ptr<Mir::Block> &block1,
                                         const std::shared_ptr<Mir::Block> &block2) const;

private:
    std::shared_ptr<ControlFlowGraph_Old> cfg = nullptr;
    std::shared_ptr<LoopAnalysis> loop_analysis = nullptr;
    std::shared_ptr<FunctionAnalysis> function_analysis = nullptr;
    std::shared_ptr<Mir::Function> current_function = nullptr;
    std::unordered_set<std::shared_ptr<Mir::Instruction>> visited_instructions;
};

// 全局值编号
// 实现全局的消除公共表达式
class GlobalValueNumbering final : public Transform {
public:
    explicit GlobalValueNumbering() : Transform("GlobalValueNumbering") {}

    static bool fold_instruction(const std::shared_ptr<Mir::Instruction> &instruction);

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    bool run_on_func(const std::shared_ptr<Mir::Function> &func);

    bool run_on_block(const std::shared_ptr<Mir::Function> &func,
                      const std::shared_ptr<Mir::Block> &block,
                      std::unordered_map<std::string, std::shared_ptr<Mir::Instruction>> &value_hashmap);

private:
    std::shared_ptr<ControlFlowGraph_Old> cfg;

    std::shared_ptr<FunctionAnalysis> func_analysis;
};

// 删除未被使用的指令
class DeadInstEliminate final : public Transform {
public:
    explicit DeadInstEliminate() : Transform("DeadInstEliminate") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    [[nodiscard]] bool remove_unused_instructions(const std::shared_ptr<Mir::Module> &module) const;

private:
    std::shared_ptr<FunctionAnalysis> func_analysis = nullptr;
};

// 删除未被调用的函数
class DeadFuncEliminate final : public Transform {
public:
    explicit DeadFuncEliminate() : Transform("DeadFuncEliminate") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

// 激进的死代码删除
class DeadCodeEliminate final : public Transform {
public:
    explicit DeadCodeEliminate() : Transform("DeadCodeEliminate") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    std::unordered_set<std::shared_ptr<Mir::Instruction>> useful_instructions_;

    std::shared_ptr<FunctionAnalysis> function_analysis_;

    // 删除指令
    void init_useful_instruction(const std::shared_ptr<Mir::Function> &function);

    void update_useful_instruction(const std::shared_ptr<Mir::Instruction> &instruction);

    // 删除全局变量
    static void dead_global_variable_eliminate(const std::shared_ptr<Mir::Module> &module);
};

// 函数无用形参删除
class DeadFuncArgEliminate final : public Transform {
public:
    explicit DeadFuncArgEliminate() : Transform("DeadFuncArgEliminate") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    std::shared_ptr<FunctionAnalysis> function_analysis_;

    void run_on_func(const std::shared_ptr<Mir::Function> &func) const;
};

// 如果该函数的返回值并未被使用，则删除该函数的返回值
class DeadReturnEliminate final : public Transform {
public:
    explicit DeadReturnEliminate() : Transform("DeadReturnEliminate") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    std::shared_ptr<FunctionAnalysis> function_analysis_;

    static void run_on_func(const std::shared_ptr<Mir::Function> &func);
};

class GlobalVariableLocalize final : public Transform {
public:
    explicit GlobalVariableLocalize() : Transform("GlobalVariableLocalize") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

class GlobalArrayLocalize final : public Transform {
public:
    explicit GlobalArrayLocalize() : Transform("GlobalArrayLocalize") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

// getelementptr 折叠：将嵌套的 getelementptr 指令链折叠为单一 getelementptr 指令
class GepFolding final : public Transform {
public:
    explicit GepFolding() : Transform("GepFolding") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    std::shared_ptr<ControlFlowGraph_Old> cfg{nullptr};

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

    std::shared_ptr<ControlFlowGraph_Old> cfg{nullptr};

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

    std::shared_ptr<ControlFlowGraph_Old> cfg{nullptr};

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
}

#endif //TRANSFORM_H

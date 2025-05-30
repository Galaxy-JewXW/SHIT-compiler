#ifndef DATAFLOW_H
#define DATAFLOW_H

#include "Pass/Transform.h"
#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Analyses/FunctionAnalysis.h"
#include "Pass/Analyses/LoopAnalysis.h"

namespace Pass {
// 自动地将 alloca 变量提升为寄存器变量，将IR转化为SSA形式
class Mem2Reg final : public Transform {
public:
    explicit Mem2Reg() : Transform("Mem2Reg") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    // 控制流图信息，用于后续基本块支配关系和变量使用/定义分析
    std::shared_ptr<ControlFlowGraph> cfg_info;
    std::shared_ptr<DominanceGraph> dom_info;
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
    std::shared_ptr<ControlFlowGraph> cfg_info = nullptr;
    std::shared_ptr<DominanceGraph> dom_info = nullptr;
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
    std::shared_ptr<DominanceGraph> dom_info;

    std::shared_ptr<FunctionAnalysis> func_analysis;
};

// 全局变量局部化
class GlobalVariableLocalize final : public Transform {
public:
    explicit GlobalVariableLocalize() : Transform("GlobalVariableLocalize") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

// 全局数组局部化
class GlobalArrayLocalize final : public Transform {
public:
    explicit GlobalArrayLocalize() : Transform("GlobalArrayLocalize") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

// 树高平衡，实现指令级并行性
class TreeHeightBalance final : public Transform {
public:
    explicit TreeHeightBalance() : Transform("TreeHeightBalance") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    static void run_on_func(const std::shared_ptr<Mir::Function> &func);
};
}

#endif //DATAFLOW_H

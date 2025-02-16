#ifndef TRANSFORM_H
#define TRANSFORM_H
#include "Pass.h"
#include "Mir/Instruction.h"

#define DEFINE_DEFAULT_TRANSFORM_CLASS(ClassName) \
class ClassName : public Transform { \
public: \
explicit ClassName() : Transform(#ClassName) {} \
protected: \
void transform(std::shared_ptr<Mir::Module> module) override; \
}

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
class ControlFlowGraph;

class Mem2Reg final : public Transform {
public:
    explicit Mem2Reg(const std::shared_ptr<ControlFlowGraph> &cfg_info) : Transform("Mem2Reg"), cfg_info{cfg_info} {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    // 控制流图信息，用于后续基本块支配关系和变量使用/定义分析
    const std::shared_ptr<ControlFlowGraph> cfg_info;
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

// 常数折叠：编译期计算常量表达式
DEFINE_DEFAULT_TRANSFORM_CLASS(ConstantFolding);
}

#endif //TRANSFORM_H

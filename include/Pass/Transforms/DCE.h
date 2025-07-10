#ifndef DCE_H
#define DCE_H

#include "Pass/Analyses/FunctionAnalysis.h"
#include "Pass/Transform.h"

namespace Pass {
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
} // namespace Pass

#endif // DCE_H

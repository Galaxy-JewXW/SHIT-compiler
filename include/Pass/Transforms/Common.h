#ifndef COMMON_H
#define COMMON_H

#include "Pass/Analyses/FunctionAnalysis.h"
#include "Pass/Transform.h"

namespace Pass {
// 对指令进行代数优化恒等式变形
class AlgebraicSimplify final : public Transform {
public:
    explicit AlgebraicSimplify() : Transform("AlgebraicSimplify") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;
};

// 标准化计算指令 "Binary"，为之后的代数变形/GVN做准备
class StandardizeBinary final : public Transform {
public:
    explicit StandardizeBinary() : Transform("StandardizeBinary") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

    void transform(const std::shared_ptr<Mir::Function> &) override;
};

// 执行在编译期内能识别出来的constexpr函数
class ConstexprFuncEval final : public Transform {
public:
    explicit ConstexprFuncEval() : Transform("ConstexprFuncEval") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    std::shared_ptr<FunctionAnalysis> func_analysis;

    [[nodiscard]] bool is_constexpr_func(const std::shared_ptr<Mir::Function> &func) const;

    [[nodiscard]] bool run_on_func(const std::shared_ptr<Mir::Function> &func) const;

    void run_on_main_func(const std::shared_ptr<Mir::Module> &module);
};
} // namespace Pass

#endif // COMMON_H

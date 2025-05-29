#ifndef COMMON_H
#define COMMON_H

#include "Pass/Transform.h"
#include "Pass/Analyses/FunctionAnalysis.h"

namespace Pass {
// 对指令进行代数优化恒等式变形
class AlgebraicSimplify final : public Transform {
public:
    explicit AlgebraicSimplify() : Transform("AlgebraicSimplify") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

// 标准化计算指令 "Binary"，为之后的代数变形/GVN做准备
class StandardizeBinary final : public Transform {
public:
    explicit StandardizeBinary() : Transform("StandardizeBinary") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;
};

// 执行在编译期内能识别出来的constexpr函数
class ConstexprFuncEval final : public Transform {
public:
    explicit ConstexprFuncEval() : Transform("ConstexprFuncEval") {}

protected:
    void transform(std::shared_ptr<Mir::Module> module) override;

private:
    std::shared_ptr<FunctionAnalysis> func_analysis;
};

}

#endif //COMMON_H

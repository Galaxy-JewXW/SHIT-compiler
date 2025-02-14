#ifndef TRANSFORM_H
#define TRANSFORM_H
#include "Pass.h"

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
    const std::shared_ptr<ControlFlowGraph> cfg_info;
};
}

#endif //TRANSFORM_H

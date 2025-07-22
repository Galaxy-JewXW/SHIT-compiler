#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "Mir/Instruction.h"
#include "Pass.h"

namespace Pass {
// 以某种方式改变和优化IR，并保证改变后的IR仍然合法有效
class Transform : public Pass {
public:
    explicit Transform(const std::string &name) : Pass(PassType::TRANSFORM, name) {}

    void run_on(const std::shared_ptr<Mir::Module> module) override { transform(module); }

    void run_on(const std::shared_ptr<Mir::Function> &function) { transform(function); }

protected:
    virtual void transform(std::shared_ptr<Mir::Module> module) = 0;

    virtual void transform(const std::shared_ptr<Mir::Function> &) { transform(Mir::Module::instance()); }
};
} // namespace Pass

#endif // TRANSFORM_H

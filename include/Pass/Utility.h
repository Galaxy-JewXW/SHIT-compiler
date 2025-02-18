#ifndef UTILITY_H
#define UTILITY_H
#include "Pass.h"

namespace Pass {
// 功能性的实用程序
class Utility : public Pass {
public:
    explicit Utility(const std::string &name) : Pass(PassType::UTILITY, name) {}

    void run_on(const std::shared_ptr<Mir::Module> module) override {
        utility(module);
    }

protected:
    virtual void utility(std::shared_ptr<Mir::Module> module) = 0;
};
}

#endif //UTILITY_H

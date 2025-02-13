#ifndef PASS_H
#define PASS_H
#include <memory>
#include <utility>
#include "Mir/Structure.h"

namespace Pass {
class Pass {
public:
    enum class PassType { ANALYSIS, TRANSFORM, UTILITY };

    virtual ~Pass() = default;

    Pass(const Pass &) = delete;

    Pass &operator=(const Pass &) = delete;

    [[nodiscard]] PassType type() const { return type_; }

    virtual void run_on_module(std::shared_ptr<Mir::Module> module) = 0;

protected:
    explicit Pass(const PassType type, std::string name) : type_{type}, name_{std::move(name)} {}

private:
    PassType type_;
    std::string name_;
};

class Analysis : public Pass {
public:
    explicit Analysis(const std::string &name) : Pass(PassType::ANALYSIS, name) {}

    void run_on_module(const std::shared_ptr<Mir::Module> module) override {
        // 强制转换为const版本保证只读访问
        const auto const_module = std::const_pointer_cast<const Mir::Module>(module);
        analyze(const_module);
    }

protected:
    // 子类必须实现的纯虚函数（只读版本）
    virtual bool analyze(std::shared_ptr<const Mir::Module> module) = 0;
};

class Transform : public Pass {
public:
    explicit Transform(const std::string &name) : Pass(PassType::TRANSFORM, name) {}

    void run_on_module(const std::shared_ptr<Mir::Module> module) override {
        transform(module);
    }

protected:
    virtual bool transform(std::shared_ptr<Mir::Module> module) = 0;
};

class Utility : public Pass {
public:
    explicit Utility(const std::string &name) : Pass(PassType::UTILITY, name) {}

    void run_on_module(const std::shared_ptr<Mir::Module> module) override {
        utility(module);
    }

protected:
    virtual void utility(std::shared_ptr<Mir::Module> module) = 0;
};

inline std::shared_ptr<Mir::Module> operator|(std::shared_ptr<Mir::Module> module,
                                              const std::shared_ptr<Pass> &pass) {
    pass->run_on_module(module);
    return module;
}
}

#endif //PASS_H

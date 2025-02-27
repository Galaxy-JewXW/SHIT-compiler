#ifndef PASS_H
#define PASS_H

#include <memory>
#include <utility>
#include "Mir/Structure.h"
#include "Utils/Log.h"

namespace Pass {
class Pass {
public:
    enum class PassType { ANALYSIS, TRANSFORM, UTILITY };

    virtual ~Pass() = default;

    Pass(Pass &&) = delete;

    Pass(const Pass &) = delete;

    Pass &operator=(Pass &&) = delete;

    Pass &operator=(const Pass &) = delete;

    [[nodiscard]] PassType type() const { return type_; }

    [[nodiscard]] std::string name() const { return name_; }

    virtual void run_on(std::shared_ptr<Mir::Module> module) = 0;

    // 创建Pass实例
    template<typename PassType, typename... Args>
    static std::shared_ptr<PassType> create(Args &&... args) {
        return std::make_shared<PassType>(std::forward<Args>(args)...);
    }

protected:
    explicit Pass(const PassType type, std::string name) : type_{type}, name_{std::move(name)} {}

private:
    PassType type_;
    std::string name_;
};

inline std::shared_ptr<Mir::Module> operator|(std::shared_ptr<Mir::Module> module,
                                              const std::shared_ptr<Pass> &pass) {
    log_info("Running pass: %s", pass->name().c_str());
    pass->run_on(module);
    return module;
}
}

void execute_O0_passes(std::shared_ptr<Mir::Module> &module);

void execute_O1_passes(std::shared_ptr<Mir::Module> &module);

#endif //PASS_H

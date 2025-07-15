#ifndef PASS_H
#define PASS_H

#include <memory>
#include <utility>

#include "Mir/Structure.h"
#include "Utils/Log.h"

namespace Pass {
class Analysis;

class Pass {
public:
    enum class PassType { ANALYSIS, TRANSFORM, UTIL };

    virtual ~Pass() = default;

    Pass(Pass &&) = delete;

    Pass(const Pass &) = delete;

    Pass &operator=(Pass &&) = delete;

    Pass &operator=(const Pass &) = delete;

    [[nodiscard]] PassType type() const { return type_; }

    [[nodiscard]] const std::string &name() const { return name_; }

    virtual void run_on(std::shared_ptr<Mir::Module> module) = 0;

    // 创建Pass实例
    template<typename PassType, typename... Args>
    static std::shared_ptr<PassType> create(Args &&...args) {
        // 检查 PassType 是否是 Pass 的派生类
        static_assert(std::is_base_of_v<Pass, PassType>, "PassType must be a derived class of Pass::Pass");
        // 检查 PassType 是否是非抽象类
        static_assert(!std::is_abstract_v<PassType>, "PassType must not be an abstract class");
        static_assert(!std::is_base_of_v<Analysis, PassType>, "Use get_analysis_result instead");
        return std::make_shared<PassType>(std::forward<Args>(args)...);
    }

protected:
    explicit Pass(const PassType type, std::string name) : type_{type}, name_{std::move(name)} {}

private:
    PassType type_;
    std::string name_;
};

inline std::shared_ptr<Mir::Module> operator|(std::shared_ptr<Mir::Module> module, const std::shared_ptr<Pass> &pass) {
    log_info("Running pass: %s", pass->name().c_str());
    pass->run_on(module);
    return module;
}
} // namespace Pass

// 对每个 Passes 类型进行检查
template<typename PassType>
struct PassChecker {
    static_assert(std::is_base_of_v<Pass::Pass, PassType>, "PassType must be a derived class of Pass::Pass");
    static_assert(!std::is_abstract_v<PassType>, "PassType must not be an abstract class");
    static_assert(!std::is_base_of_v<Pass::Analysis, PassType>, "Use get_analysis_result instead");
};

template<typename... Passes>
void apply(std::shared_ptr<Mir::Module> &module) {
    (void) std::initializer_list<int>{(PassChecker<Passes>(), 0)...};
    (..., (module = module | Pass::Pass::create<Passes>()));
}

void execute_O0_passes(std::shared_ptr<Mir::Module> &module);

void execute_O1_passes(std::shared_ptr<Mir::Module> &module);

#endif // PASS_H

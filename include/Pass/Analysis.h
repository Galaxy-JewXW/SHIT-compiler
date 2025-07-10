#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <type_traits>
#include <typeindex>
#include <utility>

#include "Pass.h"

namespace Pass {
// 计算相关IR单元的高层信息，但不对其进行修改
// 提供其它pass需要查询的信息并提供查询接口
class Analysis : public Pass {
public:
    explicit Analysis(const std::string &name) : Pass(PassType::ANALYSIS, name) {}

    void run_on(const std::shared_ptr<Mir::Module> module) override {
        // 强制转换为const版本保证只读访问
        const auto const_module = std::const_pointer_cast<const Mir::Module>(module);
        analyze(const_module);
    }

    void run_on(const std::shared_ptr<const Mir::Module> &module) { analyze(module); }

    [[nodiscard]]
    virtual bool is_dirty() const {
        return true;
    }

    [[nodiscard]]
    virtual bool is_dirty(const std::shared_ptr<Mir::Function> &function) const {
        return true;
    }

protected:
    // 子类必须实现的纯虚函数（只读版本）
    virtual void analyze(std::shared_ptr<const Mir::Module> module) = 0;
};

inline std::unordered_map<std::type_index, std::shared_ptr<Analysis>> &_analysis_results() {
    static std::unordered_map<std::type_index, std::shared_ptr<Analysis>> analysis_results;
    return analysis_results;
}

template<typename, typename = void>
struct has_set_dirty : std::false_type {};

template<typename T>
struct has_set_dirty<T,
                     std::void_t<decltype(std::declval<T>().set_dirty(std::declval<std::shared_ptr<Mir::Function>>()))>>
    : std::true_type {};

template<typename T>
inline constexpr bool has_set_dirty_v = has_set_dirty<T>::value;

template<typename T>
void set_analysis_result_dirty(const std::shared_ptr<Mir::Function> &function) {
    static_assert(std::is_base_of_v<Analysis, T>, "T must be a subclass of Analysis");
    static_assert(has_set_dirty_v<T>, "Analysis type T cannot set dirty");
    const std::type_index idx(typeid(T));
    if (const auto it = _analysis_results().find(idx);
        it != _analysis_results().end() && !it->second->is_dirty(function)) [[likely]] {
        std::static_pointer_cast<T>(it->second)->set_dirty(function);
    }
}

template<typename T, typename... Args>
std::shared_ptr<T> get_analysis_result(const std::shared_ptr<Mir::Module> module, Args &&...args) {
    static_assert(std::is_base_of_v<Analysis, T>, "T must be a subclass of Analysis");
    const std::type_index idx(typeid(T));
    if (const auto it = _analysis_results().find(idx); it != _analysis_results().end() && !it->second->is_dirty()) {
        return std::static_pointer_cast<T>(it->second);
    }
    // 检查 PassType 是否是 Pass 的派生类
    static_assert(std::is_base_of_v<Pass, T>, "PassType must be a derived class of Pass::Pass");
    // 检查 PassType 是否是非抽象类
    static_assert(!std::is_abstract_v<T>, "PassType must not be an abstract class");
    const auto analysis = std::make_shared<T>(std::forward<Args>(args)...);
    analysis->run_on(module);
    _analysis_results()[idx] = analysis;
    return analysis;
}

template<typename T, typename... Args>
std::shared_ptr<T> get_analysis_result(const std::shared_ptr<const Mir::Module> &module, Args &&...args) {
    static_assert(std::is_base_of_v<Analysis, T>, "T must be a subclass of Analysis");
    return get_analysis_result<T, Args...>(std::const_pointer_cast<Mir::Module>(module), std::forward<Args>(args)...);
}
} // namespace Pass

#endif // ANALYSIS_H

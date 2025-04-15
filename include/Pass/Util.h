#ifndef UTIL_H
#define UTIL_H
#include "Pass.h"
#include "Utils/Log.h"

namespace Pass {
// 功能性的实用程序
class Util : public Pass {
public:
    explicit Util(const std::string &name) : Pass(PassType::UTIL, name) {}

    void run_on(const std::shared_ptr<Mir::Module> module) override {
        util_impl(module);
    }

protected:
    virtual void util_impl(std::shared_ptr<Mir::Module> module) = 0;
};

template<bool update_id = false>
class EmitModule final : public Util {
public:
    explicit EmitModule() : Util("EmitModule") {}

protected:
    void util_impl(const std::shared_ptr<Mir::Module> module) override {
        if (update_id) { module->update_id(); }
        log_info("IR info as follows:\n%s", module->to_string().c_str());
    }
};

template<int log_level = LOG_ERROR>
class SetLogLevel final : public Util {
public:
    explicit SetLogLevel() : Util("SetLogLevel") {
        static_assert(log_level >= LOG_TRACE && log_level <= LOG_FATAL,
                      "log_level must be between LOG_TRACE and LOG_FATAL inclusive");
    }

protected:
    void util_impl(const std::shared_ptr<Mir::Module> module) override {
        log_set_level(log_level);
    }
};
}

// 实用函数
namespace Pass::Utils {
using namespace Mir;

// 将指令从其所在的block中移除，并移动到target之前
void move_instruction_before(const std::shared_ptr<Instruction> &instruction,
                             const std::shared_ptr<Instruction> &target);

template<typename K, typename V, typename D>
V get_or_default(const std::unordered_map<K, V> &map, const K &key, D default_value) {
    auto it = map.find(key);
    if (it != map.end()) {
        return it->second;
    }
    return static_cast<V>(default_value);
}
}

#endif //UTIL_H

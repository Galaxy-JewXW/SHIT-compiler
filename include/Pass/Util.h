#ifndef UTIL_H
#define UTIL_H

#include <cmath>
#include <limits>
#include <optional>
#include <unordered_set>

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

class CheckUninitialized final : public Util {
public:
    explicit CheckUninitialized() : Util("CheckUninitialized") {}

protected:
    void util_impl(std::shared_ptr<Mir::Module> module) override;
};
}

// 实用函数
namespace Pass::Utils {
// 模板方法
// 安全计算
template<typename Op>
std::optional<int> safe_calculate_int(const int a, const int b, Op) {
    constexpr int INT_MAX_VAL = std::numeric_limits<int>::max();
    constexpr int INT_MIN_VAL = std::numeric_limits<int>::min();

    if constexpr (std::is_same_v<Op, std::plus<>>) {
        if (b > 0 && a > INT_MAX_VAL - b) return std::nullopt;
        if (b < 0 && a < INT_MIN_VAL - b) return std::nullopt;
        return a + b;
    } else if constexpr (std::is_same_v<Op, std::minus<>>) {
        if (b < 0 && a > INT_MAX_VAL + b) return std::nullopt;
        if (b > 0 && a < INT_MIN_VAL + b) return std::nullopt;
        return a - b;
    } else if constexpr (std::is_same_v<Op, std::multiplies<>>) {
        if (a == 0 || b == 0) return 0;
        if ((a == -1 && b == INT_MIN_VAL) || (b == -1 && a == INT_MIN_VAL)) {
            return std::nullopt;
        }
        if (a > 0) {
            if (b > 0) {
                if (a > INT_MAX_VAL / b) return std::nullopt;
            } else {
                if (b < INT_MIN_VAL / a) return std::nullopt;
            }
        } else {
            if (b > 0) {
                if (a < INT_MIN_VAL / b) return std::nullopt;
            } else {
                if (a < INT_MAX_VAL / b) return std::nullopt;
            }
        }
        return a * b;
    } else if constexpr (std::is_same_v<Op, std::divides<>>) {
        if (b == 0) return std::nullopt;
        if (a == INT_MIN_VAL && b == -1) return std::nullopt;
        return a / b;
    } else if constexpr (std::is_same_v<Op, std::modulus<>>) {
        if (b == 0) return std::nullopt;
        if (a == INT_MIN_VAL && b == -1) return std::nullopt;
        return a % b;
    }
    return std::nullopt;
}

template<typename Op>
std::optional<double> safe_calculate_double(const double a, const double b, Op) {
    constexpr double DOUBLE_MAX_VAL = std::numeric_limits<double>::max();
    constexpr double DOUBLE_MIN_VAL = std::numeric_limits<double>::lowest();

    if constexpr (std::is_same_v<Op, std::plus<>>) {
        if (b > 0 && a > DOUBLE_MAX_VAL - b) return std::nullopt;
        if (b < 0 && a < DOUBLE_MIN_VAL - b) return std::nullopt;
        return a + b;
    } else if constexpr (std::is_same_v<Op, std::minus<>>) {
        if (b < 0 && a > DOUBLE_MAX_VAL + b) return std::nullopt;
        if (b > 0 && a < DOUBLE_MIN_VAL + b) return std::nullopt;
        return a - b;
    } else if constexpr (std::is_same_v<Op, std::multiplies<>>) {
        if (a == 0.0 || b == 0.0) return 0.0;
        const double abs_a = std::abs(a);
        if (const double abs_b = std::abs(b); abs_a > DOUBLE_MAX_VAL / abs_b) return std::nullopt;
        return a * b;
    } else if constexpr (std::is_same_v<Op, std::divides<>>) {
        if (b == 0.0) return std::nullopt;
        const double abs_a = std::abs(a);
        if (const double abs_b = std::abs(b); abs_b < 1.0) {
            if (abs_a > DOUBLE_MAX_VAL * abs_b) return std::nullopt;
        }
        return a / b;
    } else if constexpr (std::is_same_v<Op, std::modulus<>>) {
        if (b == 0.0) return std::nullopt;
        if (!std::isfinite(a) || !std::isfinite(b)) return std::nullopt;
        double result = std::fmod(a, b);
        if (!std::isfinite(result)) return std::nullopt;
        return result;
    }
    return std::nullopt;
}

template<typename T, typename Op>
std::optional<T> safe_cal(const T &a, const T &b, Op op) {
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, double>,
                  "Type must be int or double");

    if constexpr (std::is_same_v<T, int>) {
        return safe_calculate_int(a, b, op);
    } else {
        return safe_calculate_double(a, b, op);
    }
}

// 输出基本块集合的辅助方法
std::string format_blocks(const std::unordered_set<std::shared_ptr<Mir::Block>> &blocks);

// 将指令从其所在的block中移除，并移动到target之前
void move_instruction_before(const std::shared_ptr<Mir::Instruction> &instruction,
                             const std::shared_ptr<Mir::Instruction> &target);

// 从module中删除给定的指令集合
void delete_instruction_set(const std::shared_ptr<Mir::Module> &module,
                            const std::unordered_set<std::shared_ptr<Mir::Instruction>> &deleted_instructions);

std::optional<std::vector<std::shared_ptr<Mir::Instruction>>::iterator>
inst_as_iter(const std::shared_ptr<Mir::Instruction> &inst);
}

#endif //UTIL_H

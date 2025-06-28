#ifndef EVAL_H
#define EVAL_H

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <variant>

#include "Utils/Log.h"

struct eval_t : std::variant<int, double> {
    using base = std::variant<int, double>;
    using base::variant;

    template<typename T>
    [[nodiscard]]
    bool holds() const {
        static_assert(std::is_same_v<T, int> || std::is_same_v<T, double>,
                      "T must be int or double");
        return std::holds_alternative<T>(*this);
    }

    template<typename T>
    [[nodiscard]]
    T get() const {
        static_assert(std::is_same_v<T, int> || std::is_same_v<T, double>,
                      "T must be int or double");
        using U = std::conditional_t<std::is_same_v<T, int>, double, int>;
        if (const auto *p = std::get_if<T>(this)) {
            return *p;
        }
        if (const auto *p = std::get_if<U>(this)) {
            return static_cast<T>(*p);
        }
        log_error("Bad variant access");
    }

    template<typename T>
    static eval_t _apply(const eval_t &lhs, const eval_t &rhs, T op) {
        static_assert(std::is_invocable_v<T, int, int>, "op must be callable with (int, int)");
        static_assert(std::is_invocable_v<T, double, double>, "op must be callable with (double, double)");
        static_assert(std::is_convertible_v<std::invoke_result_t<T, int, int>, eval_t> ||
                      std::is_convertible_v<std::invoke_result_t<T, double, double>, eval_t>,
                      "op must return a type convertible to eval_t");
        if (lhs.holds<int>() && rhs.holds<int>()) {
            return op(lhs.get<int>(), rhs.get<int>());
        }
        return op(lhs.get<double>(), rhs.get<double>());
    }
};

inline eval_t operator+(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::_apply(lhs, rhs, std::plus<>());
}

inline eval_t operator-(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::_apply(lhs, rhs, std::minus<>());
}

inline eval_t operator*(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::_apply(lhs, rhs, std::multiplies<>());
}

inline eval_t operator/(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::_apply(lhs, rhs, [](auto a, auto b) {
        if (b == 0) {
            log_error("Division by zero");
        }
        return a / b;
    });
}

inline eval_t operator%(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::_apply(lhs, rhs, [](auto a, auto b) {
        if (b == 0) {
            log_error("Modulo by zero");
        }
        if constexpr (std::is_integral_v<decltype(a)> && std::is_integral_v<decltype(b)>) {
            return a % b;
        } else {
            return std::fmod(a, b);
        }
    });
}

inline eval_t max(const eval_t &a, const eval_t &b) {
    return eval_t::_apply(a, b, [](auto x, auto y) { return std::max(x, y); });
}

inline eval_t min(const eval_t &a, const eval_t &b) {
    return eval_t::_apply(a, b, [](auto x, auto y) { return std::min(x, y); });
}

#endif

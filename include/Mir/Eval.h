#ifndef EVAL_H
#define EVAL_H

#include <variant>

#include "Utils/Log.h"

struct eval_t : std::variant<int, double> {
    using base = std::variant<int, double>;
    using base::variant;

    explicit eval_t(base const &v) noexcept: base(v) {}

    explicit eval_t(base &&v) noexcept: base(v) {}

    eval_t &operator=(base const &v) noexcept {
        base::operator=(v);
        return *this;
    }

    eval_t &operator=(base &&v) noexcept {
        base::operator=(v);
        return *this;
    }

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
    static eval_t apply(const eval_t &lhs, const eval_t &rhs, T op) {
        if (lhs.holds<int>() && rhs.holds<int>()) {
            return op(lhs.get<int>(), rhs.get<int>());
        }
        return op(lhs.get<double>(), rhs.get<double>());
    }
};


inline eval_t operator+(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::apply(lhs, rhs, std::plus<>());
}

inline eval_t operator-(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::apply(lhs, rhs, std::minus<>());
}

inline eval_t operator*(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::apply(lhs, rhs, std::multiplies<>());
}

inline eval_t operator/(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::apply(lhs, rhs, [](auto a, auto b) {
        if (b == 0) {
            log_error("Division by zero");
        }
        return a / b;
    });
}

inline eval_t operator%(const eval_t &lhs, const eval_t &rhs) {
    return eval_t::apply(lhs, rhs, [](auto a, auto b) -> eval_t {
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

#endif

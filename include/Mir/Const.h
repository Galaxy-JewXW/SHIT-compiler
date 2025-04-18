#ifndef CONST_H
#define CONST_H

#include <any>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include "Value.h"
#include "Utils/Log.h"

namespace Mir {
class Const : public Value {
public:
    Const(const std::string &name, const std::shared_ptr<Type::Type> &type) : Value{name, type} {}

    ~Const() override = default;

    [[nodiscard]] virtual bool is_zero() const = 0;

    [[nodiscard]] virtual std::any get_constant_value() const = 0;

    [[nodiscard]] bool is_constant() override { return true; }

    [[nodiscard]] std::string to_string() const override { return name_; }
};

class ConstBool final : public Const {
    const int value;

    explicit ConstBool(const int value) : Const(std::to_string(value ? 1 : 0), Type::Integer::i1), value{value} {}

public:
    [[nodiscard]] bool is_zero() const override { return value == 0; }

    [[nodiscard]] std::any get_constant_value() const override { return value; }

    static std::shared_ptr<ConstBool> create(int value);

    int operator*() const {
        return value;
    }
};

class ConstInt final : public Const {
    const int value;

    explicit ConstInt(const int value, const std::shared_ptr<Type::Type> &type = Type::Integer::i32)
        : Const(std::to_string(value), type), value{value} {}

public:
    [[nodiscard]] bool is_zero() const override { return value == 0; }

    [[nodiscard]] std::any get_constant_value() const override { return value; }

    static std::shared_ptr<ConstInt> create(int value, const std::shared_ptr<Type::Type> &type = Type::Integer::i32);

    int operator+(const ConstInt &other) const {
        return value + other.value;
    }

    int operator-(const ConstInt &other) const {
        return value - other.value;
    }

    int operator*(const ConstInt &other) const {
        return value * other.value;
    }

    int operator/(const ConstInt &other) const {
        if (other.value == 0) {
            log_error("Division by zero");
        }
        return value / other.value;
    }

    int operator%(const ConstInt &other) const {
        if (other.value == 0) {
            log_error("Modulo by zero");
        }
        return value % other.value;
    }

    int operator==(const ConstInt &other) const {
        return value == other.value;
    }

    int operator!=(const ConstInt &other) const {
        return value != other.value;
    }

    int operator<(const ConstInt &other) const {
        return value < other.value;
    }

    int operator>(const ConstInt &other) const {
        return value > other.value;
    }

    int operator<=(const ConstInt &other) const {
        return value <= other.value;
    }

    int operator>=(const ConstInt &other) const {
        return value >= other.value;
    }

    int operator*() const {
        return value;
    }
};

class ConstFloat final : public Const {
    const double value;
    static constexpr double tolerance = 1e-6f;

    // static std::string gen_name(const double value) {
    //     std::ostringstream oss;
    //     oss << std::fixed << std::setprecision(6) << value << "f";
    //     return oss.str();
    // }

    static std::string gen_name(const double value) {
        uint64_t bits;
        static_assert(sizeof(value) == sizeof(bits), "Size mismatch");
        std::memcpy(&bits, &value, sizeof(value));
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << bits;
        return oss.str();
    }

    explicit ConstFloat(const double value) : Const(gen_name(value), Type::Float::f32), value{value} {}

public:
    static std::shared_ptr<ConstFloat> create(double value);

    [[nodiscard]] bool is_zero() const override {
        constexpr double tolerance = 1e-6f;
        return std::fabs(value) < tolerance;
    }

    [[nodiscard]] std::any get_constant_value() const override { return value; }

    double operator+(const ConstFloat &other) const {
        return value + other.value;
    }

    double operator-(const ConstFloat &other) const {
        return value - other.value;
    }

    double operator*(const ConstFloat &other) const {
        return value * other.value;
    }

    double operator/(const ConstFloat &other) const {
        if (other.value == 0) {
            log_error("Division by zero");
        }
        return value / other.value;
    }

    double operator%(const ConstFloat &other) const {
        if (other.value == 0) {
            log_error("Modulo by zero");
        }
        return std::fmod(value, other.value);
    }

    double operator*() const {
        return value;
    }

    int operator==(const ConstFloat &other) const {
        return std::fabs(value - other.value) < tolerance;
    }

    int operator!=(const ConstFloat &other) const {
        return std::fabs(value - other.value) >= tolerance;
    }

    int operator<(const ConstFloat &other) const {
        return value < other.value && std::fabs(value - other.value) >= tolerance;
    }

    int operator>(const ConstFloat &other) const {
        return value > other.value && std::fabs(value - other.value) >= tolerance;
    }

    int operator<=(const ConstFloat &other) const {
        return value <= other.value && std::fabs(value - other.value) >= tolerance;
    }

    int operator>=(const ConstFloat &other) const {
        return value >= other.value && std::fabs(value - other.value) >= tolerance;
    }
};
}

#endif

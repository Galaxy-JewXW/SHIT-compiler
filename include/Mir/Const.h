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

public:
    explicit ConstBool(const int value) : Const(std::to_string(value ? 1 : 0), Type::Integer::i1), value{value} {}

    [[nodiscard]] bool is_zero() const override { return value == 0; }

    [[nodiscard]] std::any get_constant_value() const override { return value; }
};

class ConstInt final : public Const {
    const int value;

public:
    explicit ConstInt(const int value) : Const(std::to_string(value), Type::Integer::i32), value{value} {}

    [[nodiscard]] bool is_zero() const override { return value == 0; }

    [[nodiscard]] std::any get_constant_value() const override { return value; }

    ConstInt operator+(const ConstInt &other) const {
        return ConstInt(value + other.value);
    }

    ConstInt operator-(const ConstInt &other) const {
        return ConstInt(value - other.value);
    }

    ConstInt operator*(const ConstInt &other) const {
        return ConstInt(value * other.value);
    }

    ConstInt operator/(const ConstInt &other) const {
        if (other.value == 0) {
            log_error("Division by zero");
        }
        return ConstInt(value / other.value);
    }

    ConstInt operator%(const ConstInt &other) const {
        if (other.value == 0) {
            log_error("Modulo by zero");
        }
        return ConstInt(value % other.value);
    }

    ConstBool operator==(const ConstInt &other) const {
        return ConstBool(value == other.value);
    }

    ConstBool operator!=(const ConstInt &other) const {
        return ConstBool(value != other.value);
    }

    ConstBool operator<(const ConstInt &other) const {
        return ConstBool(value < other.value);
    }

    ConstBool operator>(const ConstInt &other) const {
        return ConstBool(value > other.value);
    }

    ConstBool operator<=(const ConstInt &other) const {
        return ConstBool(value <= other.value);
    }

    ConstBool operator>=(const ConstInt &other) const {
        return ConstBool(value >= other.value);
    }
};

class ConstFloat final : public Const {
    const float value;

    static std::string gen_name(const float value) {
        uint32_t bits;
        static_assert(sizeof(value) == sizeof(bits), "Size mismatch");
        std::memcpy(&bits, &value, sizeof(value));
        std::ostringstream oss;
        oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << bits;
        return oss.str();
    }

public:
    explicit ConstFloat(const float value) : Const(gen_name(value), Type::Float::f32), value{value} {}

    [[nodiscard]] bool is_zero() const override {
        constexpr float tolerance = 1e-6f;
        return std::fabs(value) < tolerance;
    }

    [[nodiscard]] std::any get_constant_value() const override { return value; }

    ConstFloat operator+(const ConstFloat &other) const {
        return ConstFloat(value + other.value);
    }

    ConstFloat operator-(const ConstFloat &other) const {
        return ConstFloat(value - other.value);
    }

    ConstFloat operator*(const ConstFloat &other) const {
        return ConstFloat(value * other.value);
    }

    ConstFloat operator/(const ConstFloat &other) const {
        if (other.value == 0) {
            log_error("Division by zero");
        }
        return ConstFloat(value / other.value);
    }

    ConstFloat operator%(const ConstFloat &other) const {
        if (other.value == 0) {
            log_error("Modulo by zero");
        }
        return ConstFloat(std::fmod(value, other.value));
    }

    ConstBool operator==(const ConstFloat &other) const {
        constexpr float tolerance = 1e-6f;
        return ConstBool(std::fabs(value - other.value) < tolerance);
    }

    ConstBool operator!=(const ConstFloat &other) const {
        constexpr float tolerance = 1e-6f;
        return ConstBool(std::fabs(value - other.value) >= tolerance);
    }

    ConstBool operator<(const ConstFloat &other) const {
        constexpr float tolerance = 1e-6f;
        return ConstBool(value < other.value && std::fabs(value - other.value) >= tolerance);
    }

    ConstBool operator>(const ConstFloat &other) const {
        constexpr float tolerance = 1e-6f;
        return ConstBool(value > other.value && std::fabs(value - other.value) >= tolerance);
    }

    ConstBool operator<=(const ConstFloat &other) const {
        constexpr float tolerance = 1e-6f;
        return ConstBool(value <= other.value && std::fabs(value - other.value) >= tolerance);
    }

    ConstBool operator>=(const ConstFloat &other) const {
        constexpr float tolerance = 1e-6f;
        return ConstBool(value >= other.value && std::fabs(value - other.value) >= tolerance);
    }
};
}

#endif

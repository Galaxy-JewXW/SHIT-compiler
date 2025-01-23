#ifndef CONST_H
#define CONST_H

#include <any>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include "Value.h"

namespace Mir {
class Const : public Value {
public:
    Const(const std::string &name, const std::shared_ptr<Type::Type> &type) : Value{name, type} {}

    ~Const() override = default;

    [[nodiscard]] virtual bool is_zero() const = 0;

    [[nodiscard]] virtual std::any get_constant_value() const = 0;

    [[nodiscard]] std::string to_string() const override { return name_; }
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

    [[nodiscard]] bool is_zero() const override { return value == 0.0f; }

    [[nodiscard]] std::any get_constant_value() const override { return value; }
};

class ConstInt final : public Const {
    const int value;

public:
    explicit ConstInt(const int value) : Const(std::to_string(value), Type::Integer::i32), value{value} {}

    [[nodiscard]] bool is_zero() const override { return value == 0; }

    [[nodiscard]] std::any get_constant_value() const override { return value; }
};

class ConstBool final : public Const {
    const int value;

public:
    explicit ConstBool(const int value) : Const(value ? "true" : "false", Type::Integer::i1), value{value} {}

    [[nodiscard]] bool is_zero() const override { return value == 0; }

    [[nodiscard]] std::any get_constant_value() const override { return value; }
};
}

#endif

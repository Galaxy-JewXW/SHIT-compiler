#ifndef RV_REGISTERS_H
#define RV_REGISTERS_H

#include <string>
#include <vector>
#include <cstdint>

namespace RISCV::Instructions {
    class Value {
        public:
            [[nodiscard]] virtual std::string to_string() const = 0;

            virtual ~Value() = default;
    };

    enum class Registers {
        ZERO,
        RA,
        SP,
        GP,
        TP,
        T0,
        T1, T2,
        FP,
        S1,
        A0, A1,
        A2, A3, A4, A5, A6, A7,
        S2, S3, S4, S5, S6, S7, S8, S9, S10, S11,
        T3, T4, T5, T6,
        FT0, FT1, FT2, FT3, FT4, FT5, FT6, FT7,
    };

    [[nodiscard]] std::string reg2string(const Registers &reg);

    class Register : public Value {
        public:
            Registers reg;

            Register(Registers reg) : reg{reg} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Immediate : public Value {
        public:
            int64_t value;

            Immediate(int64_t value) : value{value} {}

            Immediate(const std::string &value) {
                this->value = std::stoll(value);
            }

            [[nodiscard]] std::string to_string() const override;
    };
}

#endif
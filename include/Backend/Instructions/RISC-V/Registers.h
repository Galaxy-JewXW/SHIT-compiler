#ifndef RV_REGISTERS_H
#define RV_REGISTERS_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "Utils/Log.h"

#pragma once

namespace RISCV::Modules {
    class FunctionField;
};

namespace RISCV::Instructions {
    class Instruction;
};

#define __STACK_START__ 0x0000_0040_007f_f820
#define __PROGRAM_START__ 0x0000_0000_0001_0430

namespace RISCV::Registers {
    enum class ABI {
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

    inline ABI operator+(ABI reg, int offset) {
        return static_cast<ABI>(static_cast<int>(reg) + offset);
    }

    inline ABI operator+(int offset, ABI reg) {
        return reg + offset;
    }

    [[nodiscard]] std::string reg2string(const ABI &reg);

    class Register {
        public:
            virtual void execute() = 0;
            virtual ~Register() = default;
            virtual std::string to_string() const = 0;
    };

    class StaticRegister : public Register {
        public:
            const ABI reg;

            explicit StaticRegister(ABI reg) : reg{reg} {
                if (reg == ABI::SP) {
                    log_warn("Using stack pointer register directly is not recommended. Use class `StackPointer` instead.");
                }
            }

            [[nodiscard]] std::string to_string() const override;

            void execute() override {}

            operator std::shared_ptr<RISCV::Registers::Register>() {
                return std::shared_ptr<RISCV::Registers::StaticRegister>(this, [](auto*) {});
            }

            std::shared_ptr<RISCV::Registers::Register> operator+(size_t offset) {
                return std::shared_ptr<RISCV::Registers::StaticRegister>(this + offset, [](auto*) {});
            }
    };

    class StackPointer : public Register {
        public:
            int64_t offset{0};
            RISCV::Modules::FunctionField *function_field{nullptr};
            std::vector<int64_t> alloc_record{};

            explicit StackPointer(RISCV::Modules::FunctionField *function_field) : function_field{function_field} {}

            void alloc_stack(int64_t size);

            void alloc_stack();

            void free_stack(int64_t size);

            void free_stack();

            void free_stack(size_t last_k);

            [[nodiscard]] std::string to_string() const override;

            void execute() override {}
    };

    #define DEFINE_REGISTER_OBJECT(name) \
        inline StaticRegister name{ABI::name};

        static DEFINE_REGISTER_OBJECT(ZERO)
        static DEFINE_REGISTER_OBJECT(RA)
        static DEFINE_REGISTER_OBJECT(GP)
        static DEFINE_REGISTER_OBJECT(TP)
        static DEFINE_REGISTER_OBJECT(T0)
        static DEFINE_REGISTER_OBJECT(T1)
        static DEFINE_REGISTER_OBJECT(T2)
        static DEFINE_REGISTER_OBJECT(FP)
        static DEFINE_REGISTER_OBJECT(S1)
        static DEFINE_REGISTER_OBJECT(A0)
        static DEFINE_REGISTER_OBJECT(A1)
        static DEFINE_REGISTER_OBJECT(A2)
        static DEFINE_REGISTER_OBJECT(A3)
        static DEFINE_REGISTER_OBJECT(A4)
        static DEFINE_REGISTER_OBJECT(A5)
        static DEFINE_REGISTER_OBJECT(A6)
        static DEFINE_REGISTER_OBJECT(A7)
        static DEFINE_REGISTER_OBJECT(S2)
        static DEFINE_REGISTER_OBJECT(S3)
        static DEFINE_REGISTER_OBJECT(S4)
        static DEFINE_REGISTER_OBJECT(S5)
        static DEFINE_REGISTER_OBJECT(S6)
        static DEFINE_REGISTER_OBJECT(S7)
        static DEFINE_REGISTER_OBJECT(S8)
        static DEFINE_REGISTER_OBJECT(S9)
        static DEFINE_REGISTER_OBJECT(S10)
        static DEFINE_REGISTER_OBJECT(S11)
        static DEFINE_REGISTER_OBJECT(T3)
        static DEFINE_REGISTER_OBJECT(T4)
        static DEFINE_REGISTER_OBJECT(T5)
        static DEFINE_REGISTER_OBJECT(T6)
        static DEFINE_REGISTER_OBJECT(FT0)
        static DEFINE_REGISTER_OBJECT(FT1)
        static DEFINE_REGISTER_OBJECT(FT2)
        static DEFINE_REGISTER_OBJECT(FT3)
        static DEFINE_REGISTER_OBJECT(FT4)
        static DEFINE_REGISTER_OBJECT(FT5)
        static DEFINE_REGISTER_OBJECT(FT6)
        static DEFINE_REGISTER_OBJECT(FT7)
}

#endif
#ifndef RV_REGISTERS_H
#define RV_REGISTERS_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "Utils/Log.h"

#define __STACK_START__ 0x0000_0040_007f_f820
#define __PROGRAM_START__ 0x0000_0000_0001_0430

namespace RISCV::Registers {
    enum class ABI : uint32_t {
        ZERO,
        RA,
        SP,
        GP,
        TP,
        T0, T1, T2,
        S0, // FP
        S1,
        A0, A1,
        A2, A3, A4, A5, A6, A7,
        S2, S3, S4, S5, S6, S7, S8, S9, S10, S11,
        T3, T4, T5, T6,
        FT0, FT1, FT2, FT3, FT4, FT5, FT6, FT7, FT8, FT9, FT10, FT11,
    };

    inline ABI operator+(ABI reg, int offset) {
        return static_cast<ABI>(static_cast<int>(reg) + offset);
    }

    inline ABI operator+(int offset, ABI reg) {
        return reg + offset;
    }

    constexpr size_t __ALL_REGS__ = static_cast<size_t>(ABI::FT11) + 1;
    constexpr size_t __ALL_INT_REGS__ = static_cast<size_t>(ABI::T6) + 1;
    constexpr size_t __ALL_FLOAT_REGS__ = static_cast<size_t>(ABI::FT11) - static_cast<size_t>(ABI::FT0) + 1;

    [[nodiscard]] std::string reg2string(const ABI &reg);
}

#endif
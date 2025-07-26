#ifndef RV_REGISTERS_H
#define RV_REGISTERS_H

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <memory>
#include "Utils/Log.h"

#define __STACK_START__ 0x0000_0040_007f_f820
#define __PROGRAM_START__ 0x0000_0000_0001_0430

namespace RISCV::Registers {
    enum class ABI : uint32_t {
        // ints
        ZERO, RA, SP, GP, TP,
        T0, T1, T2,
        S0, FP = S0, S1,
        A0, A1,
        A2, A3, A4, A5, A6, A7,
        S2, S3, S4, S5, S6, S7, S8, S9, S10, S11,
        T3, T4, T5, T6,
        // floats
        FT0, FT1, FT2, FT3, FT4, FT5, FT6, FT7,
        FS0, FS1,
        FA0, FA1, FA2, FA3, FA4, FA5, FA6, FA7,
        FS2, FS3, FS4, FS5, FS6, FS7, FS8, FS9, FS10, FS11,
        FT8, FT9, FT10, FT11
    };

    inline ABI operator+(ABI reg, int32_t offset) { return static_cast<ABI>(static_cast<int32_t>(reg) + offset); }
    inline ABI operator+(int32_t offset, ABI reg) { return reg + offset; }

    [[nodiscard]] std::string to_string(const ABI &reg);
}

namespace RISCV::Registers::Integers {
    inline const std::array<RISCV::Registers::ABI, 15> caller_saved = {
        RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T1, RISCV::Registers::ABI::T2,
        RISCV::Registers::ABI::T3, RISCV::Registers::ABI::T4, RISCV::Registers::ABI::T5,
        RISCV::Registers::ABI::T6,
        RISCV::Registers::ABI::A0, RISCV::Registers::ABI::A1, RISCV::Registers::ABI::A2,
        RISCV::Registers::ABI::A3, RISCV::Registers::ABI::A4, RISCV::Registers::ABI::A5,
        RISCV::Registers::ABI::A6, RISCV::Registers::ABI::A7,
    };
    inline const std::array<RISCV::Registers::ABI, 12> callee_saved = {
        RISCV::Registers::ABI::S0, RISCV::Registers::ABI::S1, RISCV::Registers::ABI::S2, RISCV::Registers::ABI::S3,
        RISCV::Registers::ABI::S4, RISCV::Registers::ABI::S5, RISCV::Registers::ABI::S6, RISCV::Registers::ABI::S7,
        RISCV::Registers::ABI::S8, RISCV::Registers::ABI::S9, RISCV::Registers::ABI::S10, RISCV::Registers::ABI::S11
    };
    inline const std::vector<RISCV::Registers::ABI> registers = {
        RISCV::Registers::ABI::A0, RISCV::Registers::ABI::A1, RISCV::Registers::ABI::A2, RISCV::Registers::ABI::A3,
        RISCV::Registers::ABI::A4, RISCV::Registers::ABI::A5, RISCV::Registers::ABI::A6, RISCV::Registers::ABI::A7,
        RISCV::Registers::ABI::S0, RISCV::Registers::ABI::S1, RISCV::Registers::ABI::S2, RISCV::Registers::ABI::S3,
        RISCV::Registers::ABI::S4, RISCV::Registers::ABI::S5, RISCV::Registers::ABI::S6, RISCV::Registers::ABI::S7,
        RISCV::Registers::ABI::S8, RISCV::Registers::ABI::S9, RISCV::Registers::ABI::S10, RISCV::Registers::ABI::S11,
        RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T1, RISCV::Registers::ABI::T2,
        RISCV::Registers::ABI::T3, RISCV::Registers::ABI::T4, RISCV::Registers::ABI::T5,
        RISCV::Registers::ABI::T6
    };
}

namespace RISCV::Registers::Floats {
    inline const std::array<RISCV::Registers::ABI, 20> caller_saved = {
        RISCV::Registers::ABI::FT0, RISCV::Registers::ABI::FT1, RISCV::Registers::ABI::FT2,
        RISCV::Registers::ABI::FT3, RISCV::Registers::ABI::FT4, RISCV::Registers::ABI::FT5,
        RISCV::Registers::ABI::FT6, RISCV::Registers::ABI::FT7, RISCV::Registers::ABI::FT8,
        RISCV::Registers::ABI::FT9, RISCV::Registers::ABI::FT10, RISCV::Registers::ABI::FT11,
        RISCV::Registers::ABI::FA0, RISCV::Registers::ABI::FA1, RISCV::Registers::ABI::FA2,
        RISCV::Registers::ABI::FA3, RISCV::Registers::ABI::FA4, RISCV::Registers::ABI::FA5,
        RISCV::Registers::ABI::FA6, RISCV::Registers::ABI::FA7,
    };
    inline const std::array<RISCV::Registers::ABI, 12> callee_saved = {
        RISCV::Registers::ABI::FS0, RISCV::Registers::ABI::FS1, RISCV::Registers::ABI::FS2, RISCV::Registers::ABI::FS3,
        RISCV::Registers::ABI::FS4, RISCV::Registers::ABI::FS5, RISCV::Registers::ABI::FS6, RISCV::Registers::ABI::FS7,
        RISCV::Registers::ABI::FS8, RISCV::Registers::ABI::FS9, RISCV::Registers::ABI::FS10, RISCV::Registers::ABI::FS11
    };
    inline const std::vector<RISCV::Registers::ABI> registers = {
        RISCV::Registers::ABI::FA0, RISCV::Registers::ABI::FA1, RISCV::Registers::ABI::FA2, RISCV::Registers::ABI::FA3,
        RISCV::Registers::ABI::FA4, RISCV::Registers::ABI::FA5, RISCV::Registers::ABI::FA6, RISCV::Registers::ABI::FA7,
        RISCV::Registers::ABI::FS0, RISCV::Registers::ABI::FS1, RISCV::Registers::ABI::FS2, RISCV::Registers::ABI::FS3,
        RISCV::Registers::ABI::FS4, RISCV::Registers::ABI::FS5, RISCV::Registers::ABI::FS6, RISCV::Registers::ABI::FS7,
        RISCV::Registers::ABI::FS8, RISCV::Registers::ABI::FS9, RISCV::Registers::ABI::FS10, RISCV::Registers::ABI::FS11,
        RISCV::Registers::ABI::FT0, RISCV::Registers::ABI::FT1, RISCV::Registers::ABI::FT2, RISCV::Registers::ABI::FT3,
        RISCV::Registers::ABI::FT4, RISCV::Registers::ABI::FT5, RISCV::Registers::ABI::FT6, RISCV::Registers::ABI::FT7,
        RISCV::Registers::ABI::FT8, RISCV::Registers::ABI::FT9, RISCV::Registers::ABI::FT10, RISCV::Registers::ABI::FT11
    };
}

#endif
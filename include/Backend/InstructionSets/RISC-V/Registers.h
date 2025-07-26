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

#endif
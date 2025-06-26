#include "Backend/InstructionSets/RISC-V/Registers.h"

[[nodiscard]] std::string RISCV::Registers::reg2string(const RISCV::Registers::ABI &reg) {
    switch (reg) {
        case ABI::ZERO:  return "zero";
        case ABI::RA:    return "ra";
        case ABI::SP:    return "sp";
        case ABI::GP:    return "gp";
        case ABI::TP:    return "tp";
        case ABI::T0:    return "t0";
        case ABI::T1:    return "t1";
        case ABI::T2:    return "t2";
        case ABI::FP:    return "fp";
        case ABI::S1:    return "s1";
        case ABI::A0:    return "a0";
        case ABI::A1:    return "a1";
        case ABI::A2:    return "a2";
        case ABI::A3:    return "a3";
        case ABI::A4:    return "a4";
        case ABI::A5:    return "a5";
        case ABI::A6:    return "a6";
        case ABI::A7:    return "a7";
        case ABI::S2:    return "s1";
        case ABI::S3:    return "s2";
        case ABI::S4:    return "s3";
        case ABI::S5:    return "s4";
        case ABI::S6:    return "s5";
        case ABI::S7:    return "s6";
        case ABI::S8:    return "s7";
        case ABI::S9:    return "s8";
        case ABI::S10:   return "s9";
        case ABI::S11:   return "s10";
        case ABI::T3:    return "t3";
        case ABI::T4:    return "t4";
        case ABI::T5:    return "t5";
        case ABI::T6:    return "t6";

        case ABI::FT0:   return "ft0";
        case ABI::FT1:   return "ft1";
        case ABI::FT2:   return "ft2";
        case ABI::FT3:   return "ft3";
        case ABI::FT4:   return "ft4";
        case ABI::FT5:   return "ft5";
        case ABI::FT6:   return "ft6";
        case ABI::FT7:   return "ft7";

        default: return "stack";
    }
}

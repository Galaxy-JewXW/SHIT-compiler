#include "Backend/Instructions/RISC-V/Registers.h"

[[nodiscard]] std::string RISCV::Instructions::reg2string(const RISCV::Instructions::Registers &reg) {
    switch (reg) {
        case Registers::ZERO:  return "zero";
        case Registers::RA:    return "ra";
        case Registers::SP:    return "sp";
        case Registers::GP:    return "gp";
        case Registers::TP:    return "tp";
        case Registers::T0:    return "t0";
        case Registers::T1:    return "t1";
        case Registers::T2:    return "t2";
        case Registers::FP:    return "fp";
        case Registers::S1:    return "s1";
        case Registers::A0:    return "a0";
        case Registers::A1:    return "a1";
        case Registers::A2:    return "a2";
        case Registers::A3:    return "a3";
        case Registers::A4:    return "a4";
        case Registers::A5:    return "a5";
        case Registers::A6:    return "a6";
        case Registers::A7:    return "a7";
        case Registers::S2:    return "s1";
        case Registers::S3:    return "s2";
        case Registers::S4:    return "s3";
        case Registers::S5:    return "s4";
        case Registers::S6:    return "s5";
        case Registers::S7:    return "s6";
        case Registers::S8:    return "s7";
        case Registers::S9:    return "s8";
        case Registers::S10:   return "s9";
        case Registers::S11:   return "s10";
        case Registers::T3:    return "t3";
        case Registers::T4:    return "t4";
        case Registers::T5:    return "t5";
        case Registers::T6:    return "t6";

        case Registers::FT0:   return "ft0";
        case Registers::FT1:   return "ft1";
        case Registers::FT2:   return "ft2";
        case Registers::FT3:   return "ft3";
        case Registers::FT4:   return "ft4";
        case Registers::FT5:   return "ft5";
        case Registers::FT6:   return "ft6";
        case Registers::FT7:   return "ft7";

        default: return "";
    }
}

[[nodiscard]] std::string RISCV::Instructions::Register::to_string() const {
    return RISCV::Instructions::reg2string(reg);
}

[[nodiscard]] std::string RISCV::Instructions::Immediate::to_string() const {
    return std::to_string(value);
}
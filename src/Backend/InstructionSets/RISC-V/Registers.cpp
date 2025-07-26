#include "Backend/InstructionSets/RISC-V/Registers.h"

[[nodiscard]] std::string RISCV::Registers::to_string(const RISCV::Registers::ABI &reg) {
    switch (reg) {
        case ABI::ZERO:  return "zero";
        case ABI::RA:    return "ra";
        case ABI::SP:    return "sp";
        case ABI::GP:    return "gp";
        case ABI::TP:    return "tp";
        case ABI::T0:    return "t0";
        case ABI::T1:    return "t1";
        case ABI::T2:    return "t2";
        case ABI::S0:    return "s0";
        case ABI::S1:    return "s1";
        case ABI::A0:    return "a0";
        case ABI::A1:    return "a1";
        case ABI::A2:    return "a2";
        case ABI::A3:    return "a3";
        case ABI::A4:    return "a4";
        case ABI::A5:    return "a5";
        case ABI::A6:    return "a6";
        case ABI::A7:    return "a7";
        case ABI::S2:    return "s2";
        case ABI::S3:    return "s3";
        case ABI::S4:    return "s4";
        case ABI::S5:    return "s5";
        case ABI::S6:    return "s6";
        case ABI::S7:    return "s7";
        case ABI::S8:    return "s8";
        case ABI::S9:    return "s9";
        case ABI::S10:   return "s10";
        case ABI::S11:   return "s11";
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
        case ABI::FS0:   return "fs0";
        case ABI::FS1:   return "fs1";
        case ABI::FA0:   return "fa0";
        case ABI::FA1:   return "fa1";
        case ABI::FA2:   return "fa2";
        case ABI::FA3:   return "fa3";
        case ABI::FA4:   return "fa4";
        case ABI::FA5:   return "fa5";
        case ABI::FA6:   return "fa6";
        case ABI::FA7:   return "fa7";
        case ABI::FS2:   return "fs2";
        case ABI::FS3:   return "fs3";
        case ABI::FS4:   return "fs4";
        case ABI::FS5:   return "fs5";
        case ABI::FS6:   return "fs6";
        case ABI::FS7:   return "fs7";
        case ABI::FS8:   return "fs8";
        case ABI::FS9:   return "fs9";
        case ABI::FS10:  return "fs10";
        case ABI::FS11:  return "fs11";
        case ABI::FT8:   return "ft8";
        case ABI::FT9:   return "ft9";
        case ABI::FT10:  return "ft10";
        case ABI::FT11:  return "ft11";

        default: throw std::invalid_argument("Invalid RISC-V register ABI");
    }
}

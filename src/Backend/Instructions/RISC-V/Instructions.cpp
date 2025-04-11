#include "Backend/Instructions/RISC-V/Instructions.h"

namespace RISCV_Instructions {
    [[nodiscard]] std::string Add::to_string() const {
        return "add";
    }

    [[nodiscard]] std::string Ret::to_string() const {
        return "ret";
    }

    [[nodiscard]] std::string Sub::to_string() const {
        return "sub";
    };

    [[nodiscard]] std::string Mul::to_string() const {
        return "mul";
    }

    [[nodiscard]] std::string Div::to_string() const {
        return "div";
    }

    [[nodiscard]] std::string Mod::to_string() const {
        return "mod";
    }

    [[nodiscard]] std::string Jump::to_string() const {
        return "jump";
    }

    [[nodiscard]] std::string JumpIf::to_string() const {
        return "jump_if";
    }

    [[nodiscard]] std::string JumpIfNot::to_string() const {
        return "jump_if_not";
    }

    [[nodiscard]] std::string And::to_string() const {
        return "and";
    }

    [[nodiscard]] std::string Or::to_string() const {
        return "or";
    }

    [[nodiscard]] std::string Xor::to_string() const {
        return "xor";
    }

    [[nodiscard]] std::string Shl::to_string() const {
        return "shl";
    }

    [[nodiscard]] std::string Shr::to_string() const {
        return "shr";
    }

    [[nodiscard]] std::string FAdd::to_string() const {
        return "fadd";
    }

    [[nodiscard]] std::string FSub::to_string() const {
        return "fsub";
    }

    [[nodiscard]] std::string FMul::to_string() const {
        return "fmul";
    }

    [[nodiscard]] std::string FDiv::to_string() const {
        return "fdiv";
    }

    [[nodiscard]] std::string FMod::to_string() const {
        return "fmod";
    }

    [[nodiscard]] std::string Icmp::to_string() const {
        return "icmp";
    }

    [[nodiscard]] std::string Fcmp::to_string() const {
        return "fcmp";
    }

    [[nodiscard]] std::string Zext::to_string() const {
        return "zext";
    }

    [[nodiscard]] std::string Srem::to_string() const {
        return "srem";
    }

    [[nodiscard]] std::string Sdiv::to_string() const {
        return "sdiv";
    }

    [[nodiscard]] std::string Load::to_string() const {
        return "load";
    }

    [[nodiscard]] std::string Store::to_string() const {
        return "store";
    }

    [[nodiscard]] std::string GetElementPtr::to_string() const {
        return "getelementptr";
    }

    [[nodiscard]] std::string Call::to_string() const {
        return "call";
    }

    [[nodiscard]] std::string Jump::to_string() const {
        return "jump";
    }

    [[nodiscard]] std::string JumpIf::to_string() const {
        return "jump_if";
    }

    [[nodiscard]] std::string JumpIfNot::to_string() const {
        return "jump_if_not";
    }
}
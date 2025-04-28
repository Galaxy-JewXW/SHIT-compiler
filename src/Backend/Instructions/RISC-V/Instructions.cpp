#include "Backend/Instructions/RISC-V/Instructions.h"
#include "Backend/Instructions/RISC-V/Registers.h"
#include "Backend/Instructions/RISC-V/Memory.h"
#include "Utils/Log.h"
#include <ostream>

namespace RISCV::Instructions {
    [[nodiscard]] std::string Immediate::to_string() const {
        return std::to_string(value);
    }

    [[nodiscard]] std::string Sub::to_string() const {
        std::ostringstream oss;
        oss << "sub " << rd->to_string() << ", " << rs1->to_string() << ", " << rs2->to_string();
        return oss.str();
    }

    [[nodiscard]] std::string StoreDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "sd " << rs1->to_string() << ", " << imm.to_string() << "(" << rs2->to_string() << ")";
        return oss.str();
    }

    [[nodiscard]] std::string StoreWord::to_string() const {
        std::ostringstream oss;
        oss << "sw " << rs1->to_string() << ", " << imm.to_string() << "(" << rs2->to_string() << ")";
        return oss.str();
    }

    [[nodiscard]] std::string LoadDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "ld " << rd->to_string() << ", " << imm.to_string() << "(" << rs1->to_string() << ")";
        return oss.str();
    }

    [[nodiscard]] std::string LoadWord::to_string() const {
        std::ostringstream oss;
        oss << "lw " << rd->to_string() << ", " << imm.to_string() << "(" << rs1->to_string() << ")";
        return oss.str();
    }

    [[nodiscard]] std::string Add::to_string() const {
        std::ostringstream oss;
        oss << "add " << rd->to_string() << ", " << rs1->to_string() << ", " << rs2->to_string();
        return oss.str();
    }

    [[nodiscard]] std::string AddImmediate::to_string() const {
        std::ostringstream oss;
        oss << "addi " << rd->to_string() << ", " << rs1->to_string() << ", " << imm.to_string();
        return oss.str();
    }

    [[nodiscard]] std::string LoadAddress::to_string() const {
        std::ostringstream oss;
        oss << "la " << rd->to_string() << ", " << label;
        return oss.str();
    }

    [[nodiscard]] std::string Call::to_string() const {
        std::ostringstream oss;
        oss << "call " << function_name;
        return oss.str();
    }

    [[nodiscard]] std::string Ecall::to_string() const {
        return "ecall";
    }

    [[nodiscard]] std::string Ret::to_string() const {
        return "ret";
    }

    [[nodiscard]] std::string LoadImmediate::to_string() const {
        std::ostringstream oss;
        oss << "li " << rd->to_string() << ", " << imm.to_string();
        return oss.str();
    }

    [[nodiscard]] std::string Label::to_string() const {
        return label + ":";
    }

    [[nodiscard]] std::string Mul::to_string() const {
        std::ostringstream oss;
        oss << "mul " << rd->to_string() << ", " << rs1->to_string() << ", " << rs2->to_string();
        return oss.str();
    }

    [[nodiscard]] std::string Div::to_string() const {
        std::ostringstream oss;
        oss << "div " << rd->to_string() << ", " << rs1->to_string() << ", " << rs2->to_string();
        return oss.str();
    }

    [[nodiscard]] std::string Mod::to_string() const {
        std::ostringstream oss;
        oss << "mod " << rd->to_string() << ", " << rs1->to_string() << ", " << rs2->to_string();
        return oss.str();
    }

    [[nodiscard]] std::string Jump::to_string() const {
        std::ostringstream oss;
        oss << "j " << label;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnEqual::to_string() const {
        std::ostringstream oss;
        oss << "beq " << rs1->to_string() << ", " << rs2->to_string() << ", " << label;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnNotEqual::to_string() const {
        std::ostringstream oss;
        oss << "bne " << rs1->to_string() << ", " << rs2->to_string() << ", " << label;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnLessThan::to_string() const {
        std::ostringstream oss;
        oss << "blt " << rs1->to_string() << ", " << rs2->to_string() << ", " << label;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnLessThanOrEqual::to_string() const {
        std::ostringstream oss;
        oss << "ble " << rs1->to_string() << ", " << rs2->to_string() << ", " << label;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnGreaterThan::to_string() const {
        std::ostringstream oss;
        oss << "bgt " << rs1->to_string() << ", " << rs2->to_string() << ", " << label;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnGreaterThanOrEqual::to_string() const {
        std::ostringstream oss;
        oss << "bge " << rs1->to_string() << ", " << rs2->to_string() << ", " << label;
        return oss.str();
    }
}
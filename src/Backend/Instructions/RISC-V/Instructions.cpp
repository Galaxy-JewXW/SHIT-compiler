#include "Backend/Instructions/RISC-V/Instructions.h"
#include "Utils/Log.h"
#include "Backend/Instructions/RISC-V/Memory.h"
#include <ostream>

namespace RISCV::Instructions {
    [[nodiscard]] std::string Sub::to_string() const {
        std::ostringstream oss;
        oss << "sub " << rd.to_string() << ", " << rs1.to_string() << ", " << rs2.to_string();
        return oss.str();
    }

    [[nodiscard]] std::string StoreDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "sd " << rs1.to_string() << ", " << imm.to_string() << "(" << rs2.to_string() << ")";
        return oss.str();
    }

    [[nodiscard]] std::string LoadDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "ld " << rd.to_string() << ", " << imm.to_string() << "(" << rs1.to_string() << ")";
        return oss.str();
    }

    [[nodiscard]] std::string Add::to_string() const {
        std::ostringstream oss;
        oss << "add " << rd.to_string() << ", " << rs1.to_string() << ", " << rs2.to_string();
        return oss.str();
    }

    [[nodiscard]] std::string Addi::to_string() const {
        std::ostringstream oss;
        oss << "addi " << rd.to_string() << ", " << rs1.to_string() << ", " << imm.to_string();
        return oss.str();
    }

    [[nodiscard]] std::string LoadAddress::to_string() const {
        std::ostringstream oss;
        oss << "la " << rd.to_string() << ", " << label;
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
}
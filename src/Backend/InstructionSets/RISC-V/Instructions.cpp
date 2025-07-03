#include "Backend/InstructionSets/RISC-V/Instructions.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Utils/Log.h"
#include <ostream>
#include <sstream>
#include <iomanip>

namespace RISCV::Instructions {
    [[nodiscard]] std::string Sub::to_string() const {
        std::ostringstream oss;
        oss << "sub " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    [[nodiscard]] std::string StoreDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "sd " << RISCV::Registers::reg2string(rs2) << ", " << imm << "(" << RISCV::Registers::reg2string(rs1) << ")";
        return oss.str();
    }

    [[nodiscard]] std::string StoreWord::to_string() const {
        std::ostringstream oss;
        oss << "sw " << RISCV::Registers::reg2string(rs2) << ", " << imm << "(" << RISCV::Registers::reg2string(rs1) << ")";
        return oss.str();
    }

    [[nodiscard]] std::string LoadDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "ld " << RISCV::Registers::reg2string(rd) << ", " << imm << "(" << RISCV::Registers::reg2string(rs1) << ")";
        return oss.str();
    }

    [[nodiscard]] std::string LoadWord::to_string() const {
        std::ostringstream oss;
        oss << "lw " << RISCV::Registers::reg2string(rd) << ", " << imm << "(" << RISCV::Registers::reg2string(rs1) << ")";
        return oss.str();
    }

    [[nodiscard]] std::string Add::to_string() const {
        std::ostringstream oss;
        oss << "add " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    [[nodiscard]] std::string AddImmediate::to_string() const {
        std::ostringstream oss;
        oss << "addi " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << imm;
        return oss.str();
    }

    [[nodiscard]] std::string LoadAddress::to_string() const {
        std::ostringstream oss;
        oss << "la " << RISCV::Registers::reg2string(rd) << ", " << variable->name;
        return oss.str();
    }

    [[nodiscard]] std::string LoadWordFromStack::to_string() const {
        return std::make_shared<Instructions::LoadWord>(rd, RISCV::Registers::ABI::SP, stack->stack_size - stack->stack_index[variable->name] + offset)->to_string();
    }

    [[nodiscard]] std::string Call::to_string() const {
        std::ostringstream oss;
        oss << "call " << function->name;
        return oss.str();
    }

    [[nodiscard]] std::string Ret::to_string() const {
        return "ret";
    }

    [[nodiscard]] std::string LoadImmediate::to_string() const {
        std::ostringstream oss;
        oss << "li " << RISCV::Registers::reg2string(rd) << ", " << imm;
        return oss.str();
    }

    [[nodiscard]] std::string Mul::to_string() const {
        std::ostringstream oss;
        oss << "mul " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    [[nodiscard]] std::string Div::to_string() const {
        std::ostringstream oss;
        oss << "div " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    [[nodiscard]] std::string Mod::to_string() const {
        std::ostringstream oss;
        oss << "rem " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    [[nodiscard]] std::string Jump::to_string() const {
        std::ostringstream oss;
        oss << "j " << target_block->name;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnEqual::to_string() const {
        std::ostringstream oss;
        oss << "beq " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->name;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnNotEqual::to_string() const {
        std::ostringstream oss;
        oss << "bne " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->name;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnLessThan::to_string() const {
        std::ostringstream oss;
        oss << "blt " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->name;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnLessThanOrEqual::to_string() const {
        std::ostringstream oss;
        oss << "ble " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->name;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnGreaterThan::to_string() const {
        std::ostringstream oss;
        oss << "bgt " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->name;
        return oss.str();
    }

    [[nodiscard]] std::string BranchOnGreaterThanOrEqual::to_string() const {
        std::ostringstream oss;
        oss << "bge " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->name;
        return oss.str();
    }

    [[nodiscard]] std::string AllocStack::to_string() const {
        return std::make_shared<Instructions::AddImmediate>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, -stack->stack_size)->to_string();;
    }

    [[nodiscard]] std::string StoreRA::to_string() const {
        return std::make_shared<Instructions::StoreDoubleword>(RISCV::Registers::ABI::RA, RISCV::Registers::ABI::SP, stack->stack_size - stack->RA_SIZE)->to_string();
    }

    [[nodiscard]] std::string StoreSP::to_string() const {
        return std::make_shared<Instructions::StoreDoubleword>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, stack->stack_size - stack->RA_SIZE - stack->SP_SIZE)->to_string();
    }
}
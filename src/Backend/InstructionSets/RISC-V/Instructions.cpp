#include "Backend/InstructionSets/RISC-V/Instructions.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Utils/Log.h"
#include <ostream>
#include <sstream>
#include <iomanip>

namespace RISCV::Instructions {
    std::string Sub::to_string() const {
        std::ostringstream oss;
        oss << "sub " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    std::string StoreDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "sd " << RISCV::Registers::reg2string(rs2) << ", " << imm << "(" << RISCV::Registers::reg2string(rs1) << ")";
        return oss.str();
    }

    std::string StoreWord::to_string() const {
        std::ostringstream oss;
        oss << "sw " << RISCV::Registers::reg2string(rs2) << ", " << imm << "(" << RISCV::Registers::reg2string(rs1) << ")";
        return oss.str();
    }

    std::string StoreWordToStack::to_string() const {
        std::ostringstream oss;
        oss << "sw " << RISCV::Registers::reg2string(rd) << ", " << stack->stack_size - stack->stack_index[variable->name] + offset << "(" << RISCV::Registers::reg2string(RISCV::Registers::ABI::SP) << ")";
        return oss.str();
    }

    std::string LoadDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "ld " << RISCV::Registers::reg2string(rd) << ", " << imm << "(" << RISCV::Registers::reg2string(rs1) << ")";
        return oss.str();
    }

    std::string LoadWord::to_string() const {
        std::ostringstream oss;
        oss << "lw " << RISCV::Registers::reg2string(rd) << ", " << imm << "(" << RISCV::Registers::reg2string(rs1) << ")";
        return oss.str();
    }

    std::string Add::to_string() const {
        std::ostringstream oss;
        oss << "add " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    std::string AddImmediate::to_string() const {
        std::ostringstream oss;
        oss << "addi " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << imm;
        return oss.str();
    }

    std::string LoadAddress::to_string() const {
        std::ostringstream oss;
        oss << "la " << RISCV::Registers::reg2string(rd) << ", " << variable->name;
        return oss.str();
    }

    std::string LoadWordFromStack::to_string() const {
        return std::make_shared<Instructions::LoadWord>(rd, RISCV::Registers::ABI::SP, stack->stack_size - stack->stack_index[variable->name] + offset)->to_string();
    }

    std::string Call::to_string() const {
        std::ostringstream oss;
        oss << "call " << function->name;
        return oss.str();
    }

    std::string Ret::to_string() const {
        return "ret";
    }

    std::string LoadImmediate::to_string() const {
        std::ostringstream oss;
        oss << "li " << RISCV::Registers::reg2string(rd) << ", " << imm;
        return oss.str();
    }

    std::string Mul::to_string() const {
        std::ostringstream oss;
        oss << "mul " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    std::string Div::to_string() const {
        std::ostringstream oss;
        oss << "div " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    std::string Mod::to_string() const {
        std::ostringstream oss;
        oss << "rem " << RISCV::Registers::reg2string(rd) << ", " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2);
        return oss.str();
    }

    std::string Jump::to_string() const {
        std::ostringstream oss;
        oss << "j " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnEqual::to_string() const {
        std::ostringstream oss;
        oss << "beq " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnNotEqual::to_string() const {
        std::ostringstream oss;
        oss << "bne " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnLessThan::to_string() const {
        std::ostringstream oss;
        oss << "blt " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnLessThanOrEqual::to_string() const {
        std::ostringstream oss;
        oss << "ble " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnGreaterThan::to_string() const {
        std::ostringstream oss;
        oss << "bgt " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnGreaterThanOrEqual::to_string() const {
        std::ostringstream oss;
        oss << "bge " << RISCV::Registers::reg2string(rs1) << ", " << RISCV::Registers::reg2string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string AllocStack::to_string() const {
        return std::make_shared<Instructions::AddImmediate>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, -stack->stack_size)->to_string();;
    }

    std::string FreeStack::to_string() const {
        return std::make_shared<Instructions::AddImmediate>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, stack->stack_size)->to_string();;
    }

    std::string StoreRA::to_string() const {
        return std::make_shared<Instructions::StoreDoubleword>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::RA, stack->stack_size - stack->RA_SIZE)->to_string();
    }

    std::string LoadRA::to_string() const {
        return std::make_shared<Instructions::LoadDoubleword>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::RA, stack->stack_size - stack->RA_SIZE)->to_string();
    }
}
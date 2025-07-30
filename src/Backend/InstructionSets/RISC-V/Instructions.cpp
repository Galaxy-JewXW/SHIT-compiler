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
        oss << "sub " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string FSub::to_string() const {
        std::ostringstream oss;
        oss << "fsub.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string StoreDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "sd " << RISCV::Registers::to_string(rs2) << ", " << imm << "(" << RISCV::Registers::to_string(rs1) << ")";
        return oss.str();
    }

    std::string FStoreDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "fsd " << RISCV::Registers::to_string(rs2) << ", " << imm << "(" << RISCV::Registers::to_string(rs1) << ")";
        return oss.str();
    }

    std::string StoreWord::to_string() const {
        std::ostringstream oss;
        oss << "sw " << RISCV::Registers::to_string(rs2) << ", " << imm << "(" << RISCV::Registers::to_string(rs1) << ")";
        return oss.str();
    }

    std::string FStoreWord::to_string() const {
        std::ostringstream oss;
        oss << "fsw " << RISCV::Registers::to_string(rs2) << ", " << imm << "(" << RISCV::Registers::to_string(rs1) << ")";
        return oss.str();
    }

    std::string StoreWordToStack::to_string() const {
        if (Backend::Utils::type_to_size(variable->workload_type) == 8 * __BYTE__)
            return std::make_shared<Instructions::StoreDoubleword>(RISCV::Registers::ABI::SP, rd, stack->stack_size - stack->stack_index[variable->name] + offset)->to_string();
        else
            return std::make_shared<Instructions::StoreWord>(RISCV::Registers::ABI::SP, rd, stack->stack_size - stack->stack_index[variable->name] + offset)->to_string();
    }

    std::string FStoreWordToStack::to_string() const {
        if (Backend::Utils::type_to_size(variable->workload_type) == 8 * __BYTE__)
            return std::make_shared<Instructions::FStoreDoubleword>(RISCV::Registers::ABI::SP, rd, stack->stack_size - stack->stack_index[variable->name] + offset)->to_string();
        else
            return std::make_shared<Instructions::FStoreWord>(RISCV::Registers::ABI::SP, rd, stack->stack_size - stack->stack_index[variable->name] + offset)->to_string();
    }

    std::string LoadDoubleword::to_string() const {
        std::ostringstream oss;
        oss << "ld " << RISCV::Registers::to_string(rd) << ", " << imm << "(" << RISCV::Registers::to_string(rs1) << ")";
        return oss.str();
    }

    std::string LoadWord::to_string() const {
        std::ostringstream oss;
        oss << "lw " << RISCV::Registers::to_string(rd) << ", " << imm << "(" << RISCV::Registers::to_string(rs1) << ")";
        return oss.str();
    }

    std::string FLoadWord::to_string() const {
        std::ostringstream oss;
        oss << "flw " << RISCV::Registers::to_string(rd) << ", " << imm << "(" << RISCV::Registers::to_string(rs1) << ")";
        return oss.str();
    }

    std::string Add::to_string() const {
        std::ostringstream oss;
        oss << "add " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string FAdd::to_string() const {
        std::ostringstream oss;
        oss << "fadd.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string FSGNJ::to_string() const {
        std::ostringstream oss;
        oss << "fsgnj.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string AddImmediate::to_string() const {
        std::ostringstream oss;
        oss << "addi " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << imm;
        return oss.str();
    }

    std::string LoadAddress::to_string() const {
        std::ostringstream oss;
        oss << "la " << RISCV::Registers::to_string(rd) << ", " << variable->label();
        return oss.str();
    }

    std::string LoadAddressFromStack::to_string() const {
        return std::make_shared<Instructions::AddImmediate>(rd, RISCV::Registers::ABI::SP, stack->stack_size - stack->stack_index[variable->name])->to_string();
    }

    std::string LoadWordFromStack::to_string() const {
        if (Backend::Utils::type_to_size(variable->workload_type) == 8 * __BYTE__)
            return std::make_shared<Instructions::LoadDoubleword>(rd, RISCV::Registers::ABI::SP, stack->stack_size - stack->stack_index[variable->name] + offset)->to_string();
        else
            return std::make_shared<Instructions::LoadWord>(rd, RISCV::Registers::ABI::SP, stack->stack_size - stack->stack_index[variable->name] + offset)->to_string();
    }

    std::string FLoadWordFromStack::to_string() const {
        return std::make_shared<Instructions::FLoadWord>(rd, RISCV::Registers::ABI::SP, stack->stack_size - stack->stack_index[variable->name] + offset)->to_string();
    }

    std::string Call::to_string() const {
        std::ostringstream oss;
        oss << "call " << function_name;
        return oss.str();
    }

    std::string Ret::to_string() const {
        return "ret";
    }

    std::string LoadImmediate::to_string() const {
        std::ostringstream oss;
        oss << "li " << RISCV::Registers::to_string(rd) << ", " << imm;
        return oss.str();
    }

    std::string Mul::to_string() const {
        std::ostringstream oss;
        oss << "mul " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string FMul::to_string() const {
        std::ostringstream oss;
        oss << "fmul.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string Div::to_string() const {
        std::ostringstream oss;
        oss << "div " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string FDiv::to_string() const {
        std::ostringstream oss;
        oss << "fdiv.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string Mod::to_string() const {
        std::ostringstream oss;
        oss << "rem " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string Jump::to_string() const {
        std::ostringstream oss;
        oss << "j " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnEqual::to_string() const {
        std::ostringstream oss;
        oss << "beq " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnNotEqual::to_string() const {
        std::ostringstream oss;
        oss << "bne " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnLessThan::to_string() const {
        std::ostringstream oss;
        oss << "blt " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnLessThanOrEqual::to_string() const {
        std::ostringstream oss;
        oss << "ble " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnGreaterThan::to_string() const {
        std::ostringstream oss;
        oss << "bgt " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << target_block->label_name();
        return oss.str();
    }

    std::string BranchOnGreaterThanOrEqual::to_string() const {
        std::ostringstream oss;
        oss << "bge " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << target_block->label_name();
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
        return std::make_shared<Instructions::LoadDoubleword>(RISCV::Registers::ABI::RA, RISCV::Registers::ABI::SP, stack->stack_size - stack->RA_SIZE)->to_string();
    }

    std::string Fcvt_S_W::to_string() const {
        std::ostringstream oss;
        oss << "fcvt.s.w " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1);
        return oss.str();
    }

    std::string Fcvt_W_S::to_string() const {
        std::ostringstream oss;
        oss << "fcvt.w.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1);
        return oss.str();
    }

    std::string SLL::to_string() const {
        std::ostringstream oss;
        oss << "sll " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string SLLI::to_string() const {
        std::ostringstream oss;
        oss << "slli " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << imm;
        return oss.str();
    }

    std::string SRL::to_string() const {
        std::ostringstream oss;
        oss << "srl " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string SRLI::to_string() const {
        std::ostringstream oss;
        oss << "srli " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << imm;
        return oss.str();
    }
}
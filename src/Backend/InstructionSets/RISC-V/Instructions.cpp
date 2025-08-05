#include "Backend/InstructionSets/RISC-V/Instructions.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Utils/Log.h"

namespace RISCV::Instructions {
    std::string Sub::to_string() const {
        std::ostringstream oss;
        oss << "sub " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string Subw::to_string() const {
        std::ostringstream oss;
        oss << "subw " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
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
            return std::make_shared<Instructions::StoreDoubleword>(RISCV::Registers::ABI::SP, rd, stack->offset_of(variable) + offset)->to_string();
        else
            return std::make_shared<Instructions::StoreWord>(RISCV::Registers::ABI::SP, rd, stack->offset_of(variable) + offset)->to_string();
    }

    std::string FStoreWordToStack::to_string() const {
        if (Backend::Utils::type_to_size(variable->workload_type) == 8 * __BYTE__)
            return std::make_shared<Instructions::FStoreDoubleword>(RISCV::Registers::ABI::SP, rd, stack->offset_of(variable) + offset)->to_string();
        else
            return std::make_shared<Instructions::FStoreWord>(RISCV::Registers::ABI::SP, rd, stack->offset_of(variable) + offset)->to_string();
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

    std::string Addw::to_string() const {
        std::ostringstream oss;
        oss << "addw " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
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

    std::string FSGNJN::to_string() const {
        std::ostringstream oss;
        oss << "fsgnjn.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string AddImmediate::to_string() const {
        std::ostringstream oss;
        oss << "addi " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << imm;
        return oss.str();
    }

    std::string AddImmediateW::to_string() const {
        std::ostringstream oss;
        oss << "addiw " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << imm;
        return oss.str();
    }

    std::string And::to_string() const {
        std::ostringstream oss;
        oss << "and " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string Andw::to_string() const {
        std::ostringstream oss;
        oss << "andw " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string AndImmediate::to_string() const {
        std::ostringstream oss;
        oss << "andi " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << imm;
        return oss.str();
    }

    std::string AndImmediateW::to_string() const {
        std::ostringstream oss;
        oss << "andiw " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << imm;
        return oss.str();
    }

    std::string LoadAddress::to_string() const {
        std::ostringstream oss;
        oss << "la " << RISCV::Registers::to_string(rd) << ", " << variable->label();
        return oss.str();
    }

    std::string LoadAddressFromStack::to_string() const {
        if (is_12bit(stack->offset_of(variable)))
            return std::make_shared<Instructions::AddImmediate>(rd, RISCV::Registers::ABI::SP, stack->offset_of(variable))->to_string();
        else {
            std::ostringstream oss;
            oss << std::make_shared<Instructions::LoadImmediate>(rd, stack->offset_of(variable))->to_string() << "\n  ";
            oss << std::make_shared<Instructions::Add>(rd, RISCV::Registers::ABI::SP, rd)->to_string();
            return oss.str();
        }
    }

    std::string LoadWordFromStack::to_string() const {
        if (Backend::Utils::type_to_size(variable->workload_type) == 8 * __BYTE__)
            return std::make_shared<Instructions::LoadDoubleword>(rd, RISCV::Registers::ABI::SP, stack->offset_of(variable) + offset)->to_string();
        else
            return std::make_shared<Instructions::LoadWord>(rd, RISCV::Registers::ABI::SP, stack->offset_of(variable) + offset)->to_string();
    }

    std::string FLoadWordFromStack::to_string() const {
        return std::make_shared<Instructions::FLoadWord>(rd, RISCV::Registers::ABI::SP, stack->offset_of(variable) + offset)->to_string();
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
        oss << "mulw " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string Mul_SUP::to_string() const {
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
        oss << "divw " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string FDiv::to_string() const {
        std::ostringstream oss;
        oss << "fdiv.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string Mod::to_string() const {
        std::ostringstream oss;
        oss << "remw " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
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
        if (is_12bit(-stack->stack_size)) {
            return std::make_shared<Instructions::AddImmediate>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, -stack->stack_size)->to_string();
        } else {
            std::ostringstream oss;
            oss << std::make_shared<Instructions::LoadImmediate>(RISCV::Registers::ABI::T0, -stack->stack_size)->to_string() << "\n  ";
            oss << std::make_shared<Instructions::Add>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, RISCV::Registers::ABI::T0)->to_string();
            return oss.str();
        }
    }

    std::string FreeStack::to_string() const {
        if (is_12bit(stack->stack_size)) {
            return std::make_shared<Instructions::AddImmediate>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, stack->stack_size)->to_string();
        } else {
            std::ostringstream oss;
            oss << std::make_shared<Instructions::LoadImmediate>(RISCV::Registers::ABI::T0, stack->stack_size)->to_string() << "\n  ";
            oss << std::make_shared<Instructions::Add>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, RISCV::Registers::ABI::T0)->to_string();
            return oss.str();
        }
    }

    std::string StoreRA::to_string() const {
        if (is_12bit(stack->stack_size - stack->RA_SIZE)) {
            return std::make_shared<Instructions::StoreDoubleword>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::RA, stack->stack_size - stack->RA_SIZE)->to_string();
        } else {
            std::ostringstream oss;
            oss << std::make_shared<Instructions::LoadImmediate>(RISCV::Registers::ABI::T0, stack->stack_size - stack->RA_SIZE)->to_string() << "\n  ";
            oss << std::make_shared<Instructions::Add>(RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T0, RISCV::Registers::ABI::SP)->to_string() << "\n  ";
            oss << std::make_shared<Instructions::StoreDoubleword>(RISCV::Registers::ABI::T0, RISCV::Registers::ABI::RA, 0)->to_string();
            return oss.str();
        }
    }

    std::string LoadRA::to_string() const {
        if (is_12bit(stack->stack_size - stack->RA_SIZE)) {
            return std::make_shared<Instructions::LoadDoubleword>(RISCV::Registers::ABI::RA, RISCV::Registers::ABI::SP, stack->stack_size - stack->RA_SIZE)->to_string();
        } else {
            std::ostringstream oss;
            oss << std::make_shared<Instructions::LoadImmediate>(RISCV::Registers::ABI::T0, stack->stack_size - stack->RA_SIZE)->to_string() << "\n  ";
            oss << std::make_shared<Instructions::Add>(RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T0, RISCV::Registers::ABI::SP)->to_string() << "\n  ";
            oss << std::make_shared<Instructions::LoadDoubleword>(RISCV::Registers::ABI::RA, RISCV::Registers::ABI::T0, 0)->to_string();
            return oss.str();
        }
    }

    std::string Fcvt_S_W::to_string() const {
        std::ostringstream oss;
        oss << "fcvt.s.w " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1);
        return oss.str();
    }

    std::string Fcvt_W_S::to_string() const {
        std::ostringstream oss;
        oss << "fcvt.w.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", rtz";
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

    std::string F_EQUAL_S::to_string() const {
        std::ostringstream oss;
        oss << "feq.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string F_LESS_THAN_S::to_string() const {
        std::ostringstream oss;
        oss << "flt.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string F_LESS_THAN_OR_EQUAL_S::to_string() const {
        std::ostringstream oss;
        oss << "fle.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string F_GREATER_THAN_S::to_string() const {
        std::ostringstream oss;
        oss << "fgt.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string F_GREATER_THAN_OR_EQUAL_S::to_string() const {
        std::ostringstream oss;
        oss << "fge.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2);
        return oss.str();
    }

    std::string FMAdd::to_string() const {
        std::ostringstream oss;
        oss << "fmadd.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << RISCV::Registers::to_string(rs3);
        return oss.str();
    }

    std::string FNMAdd::to_string() const {
        std::ostringstream oss;
        oss << "fnmadd.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << RISCV::Registers::to_string(rs3);
        return oss.str();
    }

    std::string FMSub::to_string() const {
        std::ostringstream oss;
        oss << "fmsub.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << RISCV::Registers::to_string(rs3);
        return oss.str();
    }

    std::string FNMSub::to_string() const {
        std::ostringstream oss;
        oss << "fnmsub.s " << RISCV::Registers::to_string(rd) << ", " << RISCV::Registers::to_string(rs1) << ", " << RISCV::Registers::to_string(rs2) << ", " << RISCV::Registers::to_string(rs3);
        return oss.str();
    }
}
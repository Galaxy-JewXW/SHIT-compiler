#ifndef RV_INSTRUCTIONS_H
#define RV_INSTRUCTIONS_H

#include <cstdint>
#include <string>
#include <memory>
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/MIR/MIR.h"

namespace RISCV {
    class Stack;
    class Function;
    class Block;
}

namespace RISCV::Instructions {
    class Instruction {
        public:
            virtual ~Instruction() = default;
            explicit Instruction() = default;
            [[nodiscard]] virtual std::string to_string() const = 0;
    };

    class Utype : public Instruction {
        public:
            RISCV::Registers::ABI rd;
            int32_t imm;
            Utype(RISCV::Registers::ABI rd, int32_t imm) : rd{rd}, imm{imm} {
                if (imm < -1048576 || imm > 1048575) { // 20 bit signed range
                    throw std::out_of_range("Immediate value out of 20-bit signed range");
                }
            }
    };

    class Rtype : public Instruction {
        public:
            RISCV::Registers::ABI rd;
            RISCV::Registers::ABI rs1;
            RISCV::Registers::ABI rs2;
            Rtype(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2) : rd{rd}, rs1{rs1}, rs2{rs2} {}
    };

    class Itype : public Instruction {
        public:
            RISCV::Registers::ABI rd;
            RISCV::Registers::ABI rs1;
            int16_t imm;
            Itype(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, int16_t imm) : rd{rd}, rs1{rs1}, imm{imm} {
                if (imm < -2048 || imm > 2047) { // 12 bit signed range
                    throw std::out_of_range("Immediate value out of 12-bit signed range");
                }
            }
    };

    class Stype : public Instruction {
        public:
            RISCV::Registers::ABI rs1;
            RISCV::Registers::ABI rs2;
            int16_t imm;
            Stype(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, int16_t imm) : rs1{rs1}, rs2{rs2}, imm{imm} {
                if (imm < -2048 || imm > 2047) { // 12 bit signed range
                    throw std::out_of_range("Immediate value out of 12-bit signed range");
                }
            }
    };

    class Btype : public Instruction {
        public:
            RISCV::Registers::ABI rs1;
            RISCV::Registers::ABI rs2;
            std::shared_ptr<RISCV::Block> target_block;
            Btype(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : rs1{rs1}, rs2{rs2}, target_block{target_block} {}
    };

    class StackInstruction : public Instruction {
        public:
            std::shared_ptr<RISCV::Stack> stack;
            StackInstruction(const std::shared_ptr<RISCV::Stack> &stack) : stack{stack} {}
    };

    class LoadImmediate : public Utype {
        public:
            LoadImmediate(RISCV::Registers::ABI rd, int32_t imm) : Utype{rd, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Add : public Rtype {
        public:
            Add(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class AddImmediate : public Itype {
        public:
            AddImmediate(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, int16_t imm) : Itype{rd, rs1, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Sub : public Rtype {
        public:
            Sub(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class StoreDoubleword : public Stype {
        public:
            StoreDoubleword(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, int16_t imm) : Stype{rs1, rs2, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class StoreWord : public Stype {
        public:
            StoreWord(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, int16_t imm) : Stype{rs1, rs2, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class LoadDoubleword : public Itype {
        public:
            LoadDoubleword(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, int16_t imm) : Itype{rd, rs1, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class LoadWord : public Itype {
        public:
            LoadWord(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, int16_t imm) : Itype{rd, rs1, imm} {}
            LoadWord(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1) : Itype{rd, rs1, 0} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class LoadWordFromStack : public StackInstruction {
        public:
            RISCV::Registers::ABI rd;
            std::shared_ptr<Backend::MIR::Variable> variable;
            int64_t offset{0};
            LoadWordFromStack(RISCV::Registers::ABI rd, std::shared_ptr<Backend::MIR::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack) : StackInstruction(stack), rd{rd}, variable{variable} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class LoadAddress : public Instruction {
        public:
            RISCV::Registers::ABI rd;
            std::shared_ptr<Backend::MIR::Variable> variable;
            LoadAddress(RISCV::Registers::ABI rd, std::shared_ptr<Backend::MIR::Variable> &variable) : rd{rd} , variable{variable} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Mul : public Rtype{
        public:
            Mul(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Div : public Rtype {
        public:
            Div(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Mod : public Rtype {
        public:
            Mod(RISCV::Registers::ABI rd, RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Ret final : public Instruction {
        public:
            [[nodiscard]] std::string to_string() const override;
    };

    class Call final : public Instruction {
        public:
            std::shared_ptr<RISCV::Function> function;

            Call(const std::shared_ptr<RISCV::Function> &function) : function{function} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Jump final : public Instruction {
        public:
            std::shared_ptr<RISCV::Block> target_block;

            Jump(const std::shared_ptr<RISCV::Block> &target_block) : target_block{target_block} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnEqual final : public Btype {
        public:
            BranchOnEqual(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnNotEqual final : public Btype {
        public:
            BranchOnNotEqual(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnLessThan final : public Btype {
        public:
            BranchOnLessThan(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnLessThanOrEqual final : public Btype {
        public:
            BranchOnLessThanOrEqual(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnGreaterThan final : public Btype {
        public:
            BranchOnGreaterThan(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnGreaterThanOrEqual final : public Btype {
        public:
            BranchOnGreaterThanOrEqual(RISCV::Registers::ABI rs1, RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class AllocStack final : public StackInstruction {
        public:
            explicit AllocStack(const std::shared_ptr<RISCV::Stack> &stack) : StackInstruction{stack} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class StoreRA final : public StackInstruction {
        public:
            explicit StoreRA(const std::shared_ptr<RISCV::Stack> &stack) : StackInstruction{stack} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class StoreSP final : public StackInstruction {
        public:
            explicit StoreSP(const std::shared_ptr<RISCV::Stack> &stack) : StackInstruction{stack} {}
            [[nodiscard]] std::string to_string() const override;
    };
}

#endif
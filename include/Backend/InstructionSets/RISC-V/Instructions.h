#ifndef RV_INSTRUCTIONS_H
#define RV_INSTRUCTIONS_H

#include <cstdint>
#include <string>
#include <memory>
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/LIR/LIR.h"

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
            const RISCV::Registers::ABI rd;
            int32_t imm;
            Utype(const RISCV::Registers::ABI rd, int32_t imm) : rd{rd}, imm{imm} {
                if (imm < -1048576 || imm > 1048575) { // 20 bit signed range
                    throw std::out_of_range("Immediate value out of 20-bit signed range");
                }
            }
    };

    class Rtype : public Instruction {
        public:
            const RISCV::Registers::ABI rd;
            const RISCV::Registers::ABI rs1;
            const RISCV::Registers::ABI rs2;
            Rtype(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : rd{rd}, rs1{rs1}, rs2{rs2} {}
    };

    class Itype : public Instruction {
        public:
            const RISCV::Registers::ABI rd;
            const RISCV::Registers::ABI rs1;
            int32_t imm;
            Itype(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : rd{rd}, rs1{rs1}, imm{imm} {
                if (imm < -2048 || imm > 2047) { // 12 bit signed range
                    throw std::out_of_range("Immediate value out of 12-bit signed range");
                }
            }
    };

    class Stype : public Instruction {
        public:
            const RISCV::Registers::ABI rs1;
            const RISCV::Registers::ABI rs2;
            int32_t imm;
            Stype(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, int32_t imm) : rs1{rs1}, rs2{rs2}, imm{imm} {
                if (imm < -2048 || imm > 2047) { // 12 bit signed range
                    throw std::out_of_range("Immediate value out of 12-bit signed range");
                }
            }
    };

    class Btype : public Instruction {
        public:
            const RISCV::Registers::ABI rs1;
            const RISCV::Registers::ABI rs2;
            std::shared_ptr<RISCV::Block> target_block;
            Btype(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : rs1{rs1}, rs2{rs2}, target_block{target_block} {}
    };

    class StackInstruction : public Instruction {
        public:
            std::shared_ptr<RISCV::Stack> stack;
            StackInstruction(const std::shared_ptr<RISCV::Stack> &stack) : stack{stack} {}
    };

    class LoadImmediate : public Utype {
        public:
            LoadImmediate(const RISCV::Registers::ABI rd, int32_t imm) : Utype{rd, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Add : public Rtype {
        public:
            Add(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class AddImmediate : public Itype {
        public:
            AddImmediate(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : Itype{rd, rs1, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class SubImmediate : public AddImmediate {
        public:
            SubImmediate(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : AddImmediate{rd, rs1, -imm} {}
    };

    class Sub : public Rtype {
        public:
            Sub(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class StoreDoubleword : public Stype {
        public:
            StoreDoubleword(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, int32_t imm) : Stype{rs1, rs2, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class StoreWordToStack : public StackInstruction {
        public:
            const RISCV::Registers::ABI rd;
            std::shared_ptr<Backend::Variable> variable;
            int64_t offset{0};
            StoreWordToStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack) : StackInstruction(stack), rd{rd}, variable{variable} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class StoreWord : public Stype {
        public:
            StoreWord(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, int32_t imm) : Stype{rs1, rs2, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class LoadDoubleword : public Itype {
        public:
            LoadDoubleword(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : Itype{rd, rs1, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class LoadWord : public Itype {
        public:
            LoadWord(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : Itype{rd, rs1, imm} {}
            LoadWord(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1) : Itype{rd, rs1, 0} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class LoadWordFromStack : public StackInstruction {
        public:
            const RISCV::Registers::ABI rd;
            std::shared_ptr<Backend::Variable> variable;
            int64_t offset{0};
            LoadWordFromStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack) : StackInstruction(stack), rd{rd}, variable{variable} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class LoadAddress : public Instruction {
        public:
            const RISCV::Registers::ABI rd;
            std::shared_ptr<Backend::Variable> variable;
            LoadAddress(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable) : rd{rd} , variable{variable} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Mul : public Rtype{
        public:
            Mul(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Div : public Rtype {
        public:
            Div(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Mod : public Rtype {
        public:
            Mod(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Ret final : public Instruction {
        public:
            [[nodiscard]] std::string to_string() const override;
    };

    class Call final : public Instruction {
        public:
            const std::string function_name;

            Call(const std::string &function_name) : function_name{function_name} {}

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
            BranchOnEqual(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnNotEqual final : public Btype {
        public:
            BranchOnNotEqual(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnLessThan final : public Btype {
        public:
            BranchOnLessThan(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnLessThanOrEqual final : public Btype {
        public:
            BranchOnLessThanOrEqual(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnGreaterThan final : public Btype {
        public:
            BranchOnGreaterThan(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnGreaterThanOrEqual final : public Btype {
        public:
            BranchOnGreaterThanOrEqual(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class AllocStack final : public StackInstruction {
        public:
            explicit AllocStack(const std::shared_ptr<RISCV::Stack> &stack) : StackInstruction{stack} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FreeStack final : public StackInstruction {
        public:
            explicit FreeStack(const std::shared_ptr<RISCV::Stack> &stack) : StackInstruction{stack} {}
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

    class LoadRA final : public StackInstruction {
        public:
            explicit LoadRA(const std::shared_ptr<RISCV::Stack> &stack) : StackInstruction{stack} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class LoadSP final : public StackInstruction {
        public:
            explicit LoadSP(const std::shared_ptr<RISCV::Stack> &stack) : StackInstruction{stack} {}
            [[nodiscard]] std::string to_string() const override;
    };
}

#endif
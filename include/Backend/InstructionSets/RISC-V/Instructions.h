#ifndef RV_INSTRUCTIONS_H
#define RV_INSTRUCTIONS_H

#include <cstdint>
#include <string>
#include <memory>
#include <ostream>
#include <sstream>
#include <iomanip>
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/LIR/LIR.h"
#include "Backend/VariableTypes.h"

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
        protected:
            static inline bool is_12bit(int32_t value) {
                return value >= -2048 && value <= 2047;
            }

            static inline bool is_20bit(int32_t value) {
                return value >= -1048576 && value <= 1048575;
            }
    };

    class Utype : public Instruction {
        public:
            const RISCV::Registers::ABI rd;
            int32_t imm;
            Utype(const RISCV::Registers::ABI rd, int32_t imm) : rd{rd}, imm{imm} {
                if (!is_20bit(imm)) {
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

    class R4type : public Rtype {
        public:
            const RISCV::Registers::ABI rs3;
            R4type(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const RISCV::Registers::ABI rs3) : Rtype{rd, rs1, rs2}, rs3{rs3} {}
    };

    class Itype : public Instruction {
        public:
            const RISCV::Registers::ABI rd;
            const RISCV::Registers::ABI rs1;
            int32_t imm;
            Itype(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : rd{rd}, rs1{rs1}, imm{imm} {
                if (!is_12bit(imm)) {
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
                if (!is_12bit(imm)) {
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

    class LoadImmediate : public Instruction {
        public:
            const RISCV::Registers::ABI rd;
            int32_t imm;
            LoadImmediate(const RISCV::Registers::ABI rd, int32_t imm) : rd{rd}, imm{imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Add : public Rtype {
        public:
            Add(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Addw : public Rtype {
        public:
            Addw(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FAdd : public Rtype {
        public:
            FAdd(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FSGNJ : public Rtype {
        public:
            FSGNJ(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FSGNJN : public Rtype {
        public:
            FSGNJN(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Fmv : public FSGNJ {
        public:
            Fmv(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs) : FSGNJ{rd, rs, rs} {}
    };

    class AddImmediate : public Itype {
        public:
            AddImmediate(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : Itype{rd, rs1, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class And : public Rtype {
    public:
        And(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
        [[nodiscard]] std::string to_string() const override;
    };

    class Andw : public Rtype {
    public:
        Andw(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
        [[nodiscard]] std::string to_string() const override;
    };

    class AndImmediate : public Itype {
        public:
            AndImmediate(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : Itype{rd, rs1, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class AndImmediateW : public AndImmediate {
    public:
        AndImmediateW(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : AndImmediate{rd, rs1, imm} {}
        [[nodiscard]] std::string to_string() const override;
    };

    class AddImmediateW : public AddImmediate {
        public:
            AddImmediateW(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : AddImmediate{rd, rs1, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class SubImmediate : public AddImmediate {
        public:
            SubImmediate(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : AddImmediate{rd, rs1, -imm} {}
    };

    class SubImmediateW : public AddImmediateW {
        public:
            SubImmediateW(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : AddImmediateW{rd, rs1, -imm} {}
    };


    class Sub : public Rtype {
        public:
            Sub(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Subw : public Rtype {
        public:
            Subw(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class FSub : public Rtype {
        public:
            FSub(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class StoreDoubleword : public Stype {
        public:
            StoreDoubleword(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, int32_t imm) : Stype{rs1, rs2, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FStoreDoubleword : public Stype {
        public:
            FStoreDoubleword(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, int32_t imm) : Stype{rs1, rs2, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class StoreWordToStack : public StackInstruction {
        public:
            const RISCV::Registers::ABI rd;
            std::shared_ptr<Backend::Variable> variable;
            int64_t offset{0};
            StoreWordToStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack) : StackInstruction(stack), rd{rd}, variable{variable} {}
            StoreWordToStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack, const int64_t offset) : StackInstruction(stack), rd{rd}, variable{variable}, offset{offset} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FStoreWordToStack : public StoreWordToStack {
        public:
            FStoreWordToStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack) : StoreWordToStack{rd, variable, stack} {}
            FStoreWordToStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack, const int64_t offset) : StoreWordToStack{rd, variable, stack, offset} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class StoreWord : public Stype {
        public:
            StoreWord(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, int32_t imm) : Stype{rs1, rs2, imm} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FStoreWord : public Stype {
        public:
            FStoreWord(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, int32_t imm) : Stype{rs1, rs2, imm} {}
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

    class FLoadWord : public Itype {
        public:
            FLoadWord(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, int32_t imm) : Itype{rd, rs1, imm} {}
            FLoadWord(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1) : Itype{rd, rs1, 0} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class LoadWordFromStack : public StackInstruction {
        public:
            const RISCV::Registers::ABI rd;
            std::shared_ptr<Backend::Variable> variable;
            int64_t offset{0};
            LoadWordFromStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack) : StackInstruction(stack), rd{rd}, variable{variable} {}
            LoadWordFromStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack, const int64_t offset) : StackInstruction(stack), rd{rd}, variable{variable}, offset{offset} {}
            [[nodiscard]] virtual std::string to_string() const override;
    };

    class FLoadWordFromStack : public LoadWordFromStack {
        public:
            FLoadWordFromStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack) : LoadWordFromStack{rd, variable, stack} {}
            FLoadWordFromStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack, const int64_t offset) : LoadWordFromStack{rd, variable, stack, offset} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class LoadAddress : public Instruction {
        public:
            const RISCV::Registers::ABI rd;
            std::shared_ptr<Backend::Variable> variable;
            LoadAddress(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable) : rd{rd} , variable{variable} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class LoadAddressFromStack : public StackInstruction {
        public:
            const RISCV::Registers::ABI rd;
            std::shared_ptr<Backend::Variable> variable;
            LoadAddressFromStack(const RISCV::Registers::ABI rd, std::shared_ptr<Backend::Variable> &variable, std::shared_ptr<RISCV::Stack> &stack) : StackInstruction(stack), rd{rd}, variable{variable} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Mul : public Rtype{
        public:
            Mul(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Mul_SUP : public Rtype{
    public:
        Mul_SUP(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
        [[nodiscard]] std::string to_string() const override;
    };

    class FMul : public Rtype {
        public:
            FMul(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Div : public Rtype {
        public:
            Div(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FDiv : public Rtype {
        public:
            FDiv(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
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

    class BranchOnEqual : public Btype {
        public:
            BranchOnEqual(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnNotEqual : public Btype {
        public:
            BranchOnNotEqual(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnLessThan : public Btype {
        public:
            BranchOnLessThan(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnLessThanOrEqual : public Btype {
        public:
            BranchOnLessThanOrEqual(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnGreaterThan : public Btype {
        public:
            BranchOnGreaterThan(const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const std::shared_ptr<RISCV::Block> &target_block) : Btype{rs1, rs2, target_block} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnGreaterThanOrEqual : public Btype {
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

    class Fcvt_S_W final : public Rtype{
        public:
            Fcvt_S_W(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1) : Rtype{rd, rs1, rs1} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class Fcvt_W_S final : public Rtype{
        public:
            Fcvt_W_S(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1) : Rtype{rd, rs1, rs1} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class SLL final : public Rtype{
        public:
            SLL(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class SLLI final : public Itype {
        public:
            SLLI(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const int32_t shamt) : Itype{rd, rs1, shamt} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class SRL final : public Rtype{
        public:
            SRL(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class SRLI final : public Itype {
        public:
            SRLI(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const int32_t shamt) : Itype{rd, rs1, shamt} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class F_EQUAL_S : public Rtype {
        public:
            F_EQUAL_S(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class F_LESS_THAN_S : public Rtype {
        public:
            F_LESS_THAN_S(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class F_LESS_THAN_OR_EQUAL_S : public Rtype {
        public:
            F_LESS_THAN_OR_EQUAL_S(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class F_GREATER_THAN_S : public Rtype {
        public:
            F_GREATER_THAN_S(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class F_GREATER_THAN_OR_EQUAL_S : public Rtype {
        public:
            F_GREATER_THAN_OR_EQUAL_S(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2) : Rtype{rd, rs1, rs2} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FMAdd : public R4type {
        public:
            FMAdd(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const RISCV::Registers::ABI rs3) : R4type{rd, rs1, rs2, rs3} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FMSub : public R4type {
        public:
            FMSub(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const RISCV::Registers::ABI rs3) : R4type{rd, rs1, rs2, rs3} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FNMAdd : public R4type {
        public:
            FNMAdd(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const RISCV::Registers::ABI rs3) : R4type{rd, rs1, rs2, rs3} {}
            [[nodiscard]] std::string to_string() const override;
    };

    class FNMSub : public R4type {
        public:
            FNMSub(const RISCV::Registers::ABI rd, const RISCV::Registers::ABI rs1, const RISCV::Registers::ABI rs2, const RISCV::Registers::ABI rs3) : R4type{rd, rs1, rs2, rs3} {}
            [[nodiscard]] std::string to_string() const override;
    };
}

#endif
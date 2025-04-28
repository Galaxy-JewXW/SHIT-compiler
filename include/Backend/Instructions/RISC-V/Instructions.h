#ifndef RV_INSTRUCTIONS_H
#define RV_INSTRUCTIONS_H

#include <cstdint>
#include <string>
#include <memory>

#pragma once
namespace RISCV::Registers {
    class Register;
}

namespace RISCV::Instructions {
    class Immediate {
        public:
            int64_t value;

            Immediate(int64_t value) : value{value} {}

            Immediate(const std::string &value) {
                this->value = std::stoll(value);
            }

            [[nodiscard]] std::string to_string() const;
    };

    class Instruction {
        public:
            [[nodiscard]] virtual std::string to_string() const = 0;

            virtual ~Instruction() = default;
    };

    class Utype : public Instruction {
        public:
            std::shared_ptr<RISCV::Registers::Register> rd;
            RISCV::Instructions::Immediate imm;

            Utype(std::shared_ptr<RISCV::Registers::Register> rd, RISCV::Instructions::Immediate imm)
                : rd{rd}, imm{imm} {}
    };

    class Rtype : public Instruction {
        public:
            std::shared_ptr<RISCV::Registers::Register> rd;
            std::shared_ptr<RISCV::Registers::Register> rs1;
            std::shared_ptr<RISCV::Registers::Register> rs2;

            Rtype(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2)
                : rd{rd}, rs1{rs1}, rs2{rs2} {}
    };

    class Itype : public Instruction {
        public:
            std::shared_ptr<RISCV::Registers::Register> rd;
            std::shared_ptr<RISCV::Registers::Register> rs1;
            RISCV::Instructions::Immediate imm;

            Itype(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, RISCV::Instructions::Immediate imm)
                : rd{rd}, rs1{rs1}, imm{imm} {}
    };

    class Stype : public Instruction {
        public:
            std::shared_ptr<RISCV::Registers::Register> rs1;
            std::shared_ptr<RISCV::Registers::Register> rs2;
            RISCV::Instructions::Immediate imm;

            Stype(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, RISCV::Instructions::Immediate imm)
                : rs1{rs1}, rs2{rs2}, imm{imm} {}
    };

    class Btype : public Instruction {
        public:
            std::shared_ptr<RISCV::Registers::Register> rs1;
            std::shared_ptr<RISCV::Registers::Register> rs2;
            std::string label;

            Btype(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, std::string label)
                : rs1{rs1}, rs2{rs2}, label{label} {}
    };

    class LoadImmediate : public Utype {
        public:
            LoadImmediate(std::shared_ptr<RISCV::Registers::Register> rd, RISCV::Instructions::Immediate imm)
                : Utype{rd, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Add : public Rtype {
        public:
            Add(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2)
                : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class AddImmediate : public Itype {
        public:
            AddImmediate(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, RISCV::Instructions::Immediate imm)
                : Itype{rd, rs1, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Sub : public Rtype {
        public:
            Sub(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2)
                : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class StoreDoubleword : public Stype {
        public:
            StoreDoubleword(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, RISCV::Instructions::Immediate imm)
                : Stype{rs1, rs2, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class StoreWord : public Stype {
        public:
            StoreWord(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, RISCV::Instructions::Immediate imm)
                : Stype{rs1, rs2, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class LoadDoubleword : public Itype {
        public:
            LoadDoubleword(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, RISCV::Instructions::Immediate imm)
                : Itype{rd, rs1, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class LoadWord : public Itype {
        public:
            LoadWord(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, RISCV::Instructions::Immediate imm)
                : Itype{rd, rs1, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class LoadAddress : public Instruction {
        public:
            std::shared_ptr<RISCV::Registers::Register> rd;
            std::string label;

            LoadAddress(std::shared_ptr<RISCV::Registers::Register> rd, std::string label)
                : rd{rd} {
                    if (label[0] == '@') {
                        this->label = ".global_var_" + label.substr(1);
                    } else {
                        this->label = label;
                    }
                }

            [[nodiscard]] std::string to_string() const override;
    };

    class Mul : public Rtype{
        public:
            Mul(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2)
                : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Div : public Rtype {
        public:
            Div(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2)
                : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Mod : public Rtype {
        public:
            Mod(std::shared_ptr<RISCV::Registers::Register> rd, std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2)
                : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Ret final : public Instruction {
        public:
            [[nodiscard]] std::string to_string() const override;
    };

    class Call final : public Instruction {
        public:
            std::string function_name;

            Call(const std::string &function_name) : function_name{function_name} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Ecall final : public Instruction {
        public:
            [[nodiscard]] std::string to_string() const override;
    };

    class Label final : public Instruction {
        public:
            std::string label;

            Label(const std::string &label) : label{label} {}

            [[nodiscard]] std::string to_string() const override;

            static std::string temporary_label() {
                static int label_count = 0;
                return "..temporary_label" + std::to_string(label_count++);
            }
    };

    class Jump final : public Instruction {
        public:
            std::string label;

            Jump(const std::string &label) : label{label} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnEqual final : public Btype {
        public:
            BranchOnEqual(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, const std::string &label)
                : Btype{rs1, rs2, label} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnNotEqual final : public Btype {
        public:
            BranchOnNotEqual(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, const std::string &label)
                : Btype{rs1, rs2, label} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnLessThan final : public Btype {
        public:
            BranchOnLessThan(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, const std::string &label)
                : Btype{rs1, rs2, label} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnLessThanOrEqual final : public Btype {
        public:
            BranchOnLessThanOrEqual(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, const std::string &label)
                : Btype{rs1, rs2, label} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnGreaterThan final : public Btype {
        public:
            BranchOnGreaterThan(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, const std::string &label)
                : Btype{rs1, rs2, label} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class BranchOnGreaterThanOrEqual final : public Btype {
        public:
            BranchOnGreaterThanOrEqual(std::shared_ptr<RISCV::Registers::Register> rs1, std::shared_ptr<RISCV::Registers::Register> rs2, const std::string &label)
                : Btype{rs1, rs2, label} {}

            [[nodiscard]] std::string to_string() const override;
    };
}

#endif
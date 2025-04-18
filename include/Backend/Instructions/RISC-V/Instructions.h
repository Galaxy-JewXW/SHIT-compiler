#ifndef RV_INSTRUCTIONS_H
#define RV_INSTRUCTIONS_H

#include "Mir/Instruction.h"
#include "Mir/Value.h"
#include "Backend/Instructions/RISC-V/Registers.h"

namespace RISCV::Instructions {
    class Instruction {
        public:
            [[nodiscard]] virtual std::string to_string() const = 0;
    };

    class Utype : public Instruction {
        public:
            RISCV::Instructions::Register rd;
            RISCV::Instructions::Immediate imm;
    };

    class Rtype : public Instruction {
        public:
            RISCV::Instructions::Register rd, rs1, rs2;

            Rtype(RISCV::Instructions::Register rd, RISCV::Instructions::Register rs1, RISCV::Instructions::Register rs2)
                : rd{rd}, rs1{rs1}, rs2{rs2} {}
    };

    class Itype : public Instruction {
        public:
            RISCV::Instructions::Register rd, rs1;
            RISCV::Instructions::Immediate imm;

            Itype(RISCV::Instructions::Register rd, RISCV::Instructions::Register rs1, RISCV::Instructions::Immediate imm)
                : rd{rd}, rs1{rs1}, imm{imm} {}
    };

    class Stype : public Instruction {
        public:
            RISCV::Instructions::Register rs1, rs2;
            RISCV::Instructions::Immediate imm;

            Stype(RISCV::Instructions::Register rs1, RISCV::Instructions::Register rs2, RISCV::Instructions::Immediate imm)
                : rs1{rs1}, rs2{rs2}, imm{imm} {}
    };

    class Add : public Rtype {
        public:
            Add(RISCV::Instructions::Register rd, RISCV::Instructions::Register rs1, RISCV::Instructions::Register rs2)
                : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Addi : public Itype {
        public:
            Addi(RISCV::Instructions::Register rd, RISCV::Instructions::Register rs1, RISCV::Instructions::Immediate imm)
                : Itype{rd, rs1, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Sub : public Rtype {
        public:
            Sub(RISCV::Instructions::Register rd, RISCV::Instructions::Register rs1, RISCV::Instructions::Register rs2)
                : Rtype{rd, rs1, rs2} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class StoreDoubleword : public Stype {
        public:
            StoreDoubleword(RISCV::Instructions::Register rs1, RISCV::Instructions::Register rs2, RISCV::Instructions::Immediate imm)
                : Stype{rs1, rs2, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class LoadDoubleword : public Itype {
        public:
            LoadDoubleword(RISCV::Instructions::Register rd, RISCV::Instructions::Register rs1, RISCV::Instructions::Immediate imm)
                : Itype{rd, rs1, imm} {}

            [[nodiscard]] std::string to_string() const override;
    };

    class LoadAddress : public Instruction {
        public:
            RISCV::Instructions::Register rd;
            std::string label;

            LoadAddress(RISCV::Instructions::Register rd, std::string label)
                : rd{rd} {
                    if (label[0] == '@') {
                        this->label = ".global_var_" + label.substr(1);
                    } else {
                        this->label = label;
                    }
                }

            [[nodiscard]] std::string to_string() const override;
    };

    class Mul : public Instruction {
        public:
            // Mul(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::INTBINARY) {
            //     if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            //         log_error("Operands must be int 32");
            //     }
            // }

            [[nodiscard]] std::string to_string() const override;
    };

    class Div : public Instruction {
        public:
            // Div(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::INTBINARY) {
            //     if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            //         log_error("Operands must be int 32");
            //     }
            // }

            [[nodiscard]] std::string to_string() const override;
    };

    class Mod : public Instruction {
        public:
            // Mod(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::INTBINARY) {
            //     if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            //         log_error("Operands must be int 32");
            //     }
            // }

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

    class Ecall final : public Mir::Instruction {
        public:
            [[nodiscard]] std::string to_string() const override;
    };
}

#endif
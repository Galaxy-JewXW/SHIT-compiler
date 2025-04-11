#ifndef RV_INSTRUCTIONS_H
#define RV_INSTRUCTIONS_H

#include "Backend/Instructions/RISC-V/Instruction.h"
#include "Mir/Instruction.h"
#include "Mir/Value.h"

namespace RISCV_Instructions {
    class Add : public Instruction {
        public:
            // Add(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::INTBINARY) {
            //     if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            //         log_error("Operands must be int 32");
            //     }
            // }

            [[nodiscard]] std::string to_string() const override;
    };

    class Sub : public Instruction {
        public:
            // Sub(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::INTBINARY) {
            //     if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            //         log_error("Operands must be int 32");
            //     }
            // }

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

    class Ret final : public Mir::Instruction {
        public:
            // Ret(const std::shared_ptr<Mir::Value> &value)
            //     : Instruction("", Mir::Type::Void::void_, Mir::Operator::RET) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Call final : public Mir::Instruction {
        public:
            // Call(const std::string &name, const std::shared_ptr<Mir::Function> &function,
            //      const std::vector<std::shared_ptr<Mir::Value>> &params)
            //     : Instruction(name, function->get_type(), Mir::Operator::CALL) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Load final : public Mir::Instruction {
        public:
            // Load(const std::string &name, const std::shared_ptr<Mir::Value> &addr)
            //     : Instruction(name, std::dynamic_pointer_cast<Mir::Type::Pointer>(addr->get_type())->get_contain_type(),
            //                   Mir::Operator::LOAD) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Store final : public Mir::Instruction {
        public:
            // Store(const std::shared_ptr<Mir::Value> &addr, const std::shared_ptr<Mir::Value> &value)
            //     : Instruction("", Mir::Type::Void::void_, Mir::Operator::STORE) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class GetElementPtr final : public Mir::Instruction {
        public:
            // GetElementPtr(const std::string &name,
            //               const std::shared_ptr<Mir::Value> &addr,
            //               const std::vector<std::shared_ptr<Mir::Value>> &indexes)
            //     : Instruction(name, calc_type_(addr, indexes), Mir::Operator::GEP) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Fptosi final : public Mir::Instruction {
        public:
            // Fptosi(const std::string &name, const std::shared_ptr<Mir::Value> &value)
            //     : Instruction(name, Mir::Type::Integer::i32, Mir::Operator::FPTOSI) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Sitofp final : public Mir::Instruction {
        public:
            // Sitofp(const std::string &name, const std::shared_ptr<Mir::Value> &value)
            //     : Instruction(name, Mir::Type::Float::f32, Mir::Operator::SITOF) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class FSub final : public Mir::Instruction {
        public:
            // FSub(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::SUB) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class FAdd final : public Mir::Instruction {
        public:
            // FAdd(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::ADD) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class FMul final : public Mir::Instruction {
        public:
            // FMul(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::MUL) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class FDiv final : public Mir::Instruction {
        public:
            // FDiv(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::DIV) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class FMod final : public Mir::Instruction {
        public:
            // FMod(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, lhs->get_type(), Mir::Operator::MOD) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Icmp final : public Mir::Instruction {
        public:
            // Icmp(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, Mir::Type::Integer::i1, Mir::Operator::ICMP) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Fcmp final : public Mir::Instruction {
        public:
            // Fcmp(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, Mir::Type::Integer::i1, Mir::Operator::FCMP) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Zext final : public Mir::Instruction {
        public:
            // Zext(const std::string &name, const std::shared_ptr<Mir::Value> &value)
            //     : Instruction(name, Mir::Type::Integer::i32, Mir::Operator::ZEXT) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Srem final : public Mir::Instruction {
        public:
            // Srem(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, Mir::Type::Integer::i32, Mir::Operator::SREM) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Sdiv final : public Mir::Instruction {
        public:
            // Sdiv(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, Mir::Type::Integer::i32, Mir::Operator::SDIV) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class And final : public Mir::Instruction {
        public:
            // And(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, Mir::Type::Integer::i32, Mir::Operator::AND) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Or final : public Mir::Instruction {
        public:
            // Or(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, Mir::Type::Integer::i32, Mir::Operator::OR) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Xor final : public Mir::Instruction {
        public:
            // Xor(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, Mir::Type::Integer::i32, Mir::Operator::XOR) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Shl final : public Mir::Instruction {
        public:
            // Shl(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, Mir::Type::Integer::i32, Mir::Operator::SHL) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Shr final : public Mir::Instruction {
        public:
            // Shr(const std::string &name, const std::shared_ptr<Mir::Value> &lhs, const std::shared_ptr<Mir::Value> &rhs)
            //     : Instruction(name, Mir::Type::Integer::i32, Mir::Operator::SHR) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class Jump final : public Mir::Instruction {
        public:
            // Jump(const std::shared_ptr<Mir::Block> &block)
            //     : Instruction("", Mir::Type::Void::void_, Mir::Operator::JUMP) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class JumpIf final : public Mir::Instruction {
        public:
            // JumpIf(const std::shared_ptr<Mir::Value> &value, const std::shared_ptr<Mir::Block> &block)
            //     : Instruction("", Mir::Type::Void::void_, Mir::Operator::JUMPIF) {}

            [[nodiscard]] std::string to_string() const override;
    };

    class JumpIfNot final : public Mir::Instruction {
        public:
            // JumpIfNot(const std::shared_ptr<Mir::Value> &value, const std::shared_ptr<Mir::Block> &block)
            //     : Instruction("", Mir::Type::Void::void_, Mir::Operator::JUMPIFNOT) {}

            [[nodiscard]] std::string to_string() const override;
    };

}

#endif
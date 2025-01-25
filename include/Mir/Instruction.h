#ifndef INSTRUCTION_H
#define INSTRUCTION_H
#include "Structure.h"
#include "Value.h"

namespace Mir {
class Block;

enum class Operator {
    ALLOC,
    LOAD,
    STORE,
    GEP,
    FPTOSI,
    SITOFP,
    FCMP,
    ICMP,
    ZEXT,
    BRANCH,
    JUMP,
    RET
};

class Instruction : public User {
    std::weak_ptr<Block> block;
    Operator op;

protected:
    Instruction(const std::string &name, const std::shared_ptr<Type::Type> &type, const Operator &op)
        : User{name, type}, op{op} {}

public:
    ~Instruction() override = default;

    [[nodiscard]] std::shared_ptr<Block> get_block() const { return block.lock(); }

    void set_block(const std::shared_ptr<Block> &block) {
        this->block = block;
        block->add_instruction(std::dynamic_pointer_cast<Instruction>(shared_from_this()));
    }

    [[nodiscard]] Operator get_op() const;

    [[nodiscard]] std::string to_string() const override = 0;
};

class Alloc final : public Instruction {
public:
    Alloc(const std::string &name, const std::shared_ptr<Type::Type> &type)
        : Instruction{name, std::make_shared<Type::Pointer>(type), Operator::ALLOC} {}

    static std::shared_ptr<Alloc> create(const std::string &name, const std::shared_ptr<Type::Type> &type,
                                         const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Alloc>(name, type);
        instruction->set_block(block);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class Load final : public Instruction {
public:
    Load(const std::string &name, const std::shared_ptr<Value> &addr)
        : Instruction{
            name, std::dynamic_pointer_cast<Type::Pointer>(addr->get_type())->get_contain_type(),
            Operator::LOAD
        } {}

    static std::shared_ptr<Load> create(const std::string &name, const std::shared_ptr<Value> &addr,
                                        const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Load>(name, addr);
        instruction->set_block(block);
        instruction->add_operand(addr);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_addr() const { return operands_[0]; }

    [[nodiscard]] std::string to_string() const override;
};

class Store final : public Instruction {
public:
    Store(const std::shared_ptr<Value> &addr, const std::shared_ptr<Value> &value)
        : Instruction{"", Type::Void::void_, Operator::STORE} {}

    static std::shared_ptr<Store> create(const std::shared_ptr<Value> &addr,
                                         const std::shared_ptr<Value> &value,
                                         const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Store>(addr, value);
        instruction->set_block(block);
        instruction->add_operand(addr);
        instruction->add_operand(value);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_addr() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[1]; }

    [[nodiscard]] std::string to_string() const override;
};

class GetElementPtr final : public Instruction {
public:
    GetElementPtr(const std::string &name,
                  const std::shared_ptr<Value> &addr,
                  const std::shared_ptr<Value> &index)
        : Instruction(name, calc_type_(addr), Operator::GEP) {
        if (!index->get_type()->is_int32()) { log_error("Index must be an integer 32"); }
    }

    static std::shared_ptr<GetElementPtr> create(const std::string &name,
                                                 const std::shared_ptr<Value> &addr,
                                                 const std::shared_ptr<Value> &index,
                                                 const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<GetElementPtr>(name, addr, index);
        instruction->set_block(block);
        instruction->add_operand(addr);
        instruction->add_operand(index);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_addr() const { return operands_[0]; }
    [[nodiscard]] std::shared_ptr<Value> get_index() const { return operands_[1]; }

    [[nodiscard]] std::string to_string() const override;

private:
    static std::shared_ptr<Type::Type> calc_type_(const std::shared_ptr<Value> &addr);
};

class Fptosi final : public Instruction {
public:
    Fptosi(const std::string &name, const std::shared_ptr<Value> &value)
        : Instruction(name, Type::Integer::i32, Operator::FPTOSI) {
        if (!value->get_type()->is_float()) { log_error("Value must be a float"); }
    }

    static std::shared_ptr<Fptosi> create(const std::string &name, const std::shared_ptr<Value> &value,
                                          const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Fptosi>(name, value);
        instruction->set_block(block);
        instruction->add_operand(value);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[0]; }

    [[nodiscard]] std::string to_string() const override;
};

class Sitofp final : public Instruction {
public:
    Sitofp(const std::string &name, const std::shared_ptr<Value> &value)
        : Instruction(name, Type::Float::f32, Operator::SITOFP) {
        if (!value->get_type()->is_int32()) { log_error("Value must be an integer 32"); }
    }

    static std::shared_ptr<Sitofp> create(const std::string &name, const std::shared_ptr<Value> &value,
                                          const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Sitofp>(name, value);
        instruction->set_block(block);
        instruction->add_operand(value);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[0]; }

    [[nodiscard]] std::string to_string() const override;
};

class Fcmp final : public Instruction {
public:
    enum class Op { EQ, NE, GT, LT, GE, LE };

private:
    const Op op;

public:
    Fcmp(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : Instruction(name, Type::Integer::i1, Operator::FCMP), op{op} {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) { log_error("Operands must be a float"); }
    }

    static std::shared_ptr<Fcmp> create(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs,
                                        const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Fcmp>(name, op, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_lhs() const { return operands_[0]; }
    [[nodiscard]] std::shared_ptr<Value> get_rhs() const { return operands_[1]; }

    [[nodiscard]] std::string to_string() const override;
};

class Icmp final : public Instruction {
public:
    enum class Op { EQ, NE, GT, LT, GE, LE };

private:
    const Op op;

public:
    Icmp(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : Instruction(name, Type::Integer::i1, Operator::ICMP), op{op} {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be an integer 32");
        }
    }

    static std::shared_ptr<Icmp> create(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs,
                                        const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Icmp>(name, op, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_lhs() const { return operands_[0]; }
    [[nodiscard]] std::shared_ptr<Value> get_rhs() const { return operands_[1]; }

    [[nodiscard]] std::string to_string() const override;
};

class Zext final : public Instruction {
public:
    Zext(const std::string &name, const std::shared_ptr<Value> &value)
        : Instruction(name, Type::Integer::i32, Operator::ZEXT) {
        if (!value->get_type()->is_int1()) { log_error("Value must be an integer 1"); }
    }

    static std::shared_ptr<Zext> create(const std::string &name, const std::shared_ptr<Value> &value,
                                        const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Zext>(name, value);
        instruction->set_block(block);
        instruction->add_operand(value);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[0]; }

    [[nodiscard]] std::string to_string() const override;
};

class Terminator : public Instruction {
protected:
    Terminator(const std::shared_ptr<Type::Type> &type, const Operator op) : Instruction("", type, op) {}

    ~Terminator() override = default;
};

class Branch final : public Terminator {
public:
    Branch(const std::shared_ptr<Value> &cond, const std::shared_ptr<Block> &true_block,
           const std::shared_ptr<Block> &false_block)
        : Terminator(Type::Label::label, Operator::BRANCH) {
        if (!cond->get_type()->is_int1()) { log_error("Cond must be an integer 1"); }
    }

    static std::shared_ptr<Branch> create(const std::shared_ptr<Value> &cond, const std::shared_ptr<Block> &true_block,
                                          const std::shared_ptr<Block> &false_block,
                                          const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Branch>(cond, true_block, false_block);
        instruction->set_block(block);
        instruction->add_operand(cond);
        instruction->add_operand(true_block);
        instruction->add_operand(false_block);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_cond() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Block> get_true_block() const {
        return std::dynamic_pointer_cast<Block>(operands_[1]);
    }

    [[nodiscard]] std::shared_ptr<Block> get_false_block() const {
        return std::dynamic_pointer_cast<Block>(operands_[2]);
    }

    [[nodiscard]] std::string to_string() const override;
};

class Jump final : public Terminator {
public:
    explicit Jump(const std::shared_ptr<Block> &block) : Terminator(Type::Label::label, Operator::JUMP) {}

    static std::shared_ptr<Jump> create(const std::shared_ptr<Block> &target_block,
                                        const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Jump>(target_block);
        instruction->set_block(block);
        instruction->add_operand(target_block);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Block> get_target_block() const {
        return std::dynamic_pointer_cast<Block>(operands_[0]);
    }

    [[nodiscard]] std::string to_string() const override;
};

class Ret final : public Terminator {
public:
    explicit Ret(const std::shared_ptr<Value> &value) : Terminator(value->get_type(), Operator::RET) {
        if (value->get_type()->is_void()) {
            log_error("Value must not be void");
        }
    }

    explicit Ret() : Terminator(Type::Void::void_, Operator::RET) {}

    static std::shared_ptr<Ret> create(const std::shared_ptr<Value> &value, const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Ret>(value);
        instruction->set_block(block);
        instruction->add_operand(value);
        return instruction;
    }

    static std::shared_ptr<Ret> create(const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Ret>();
        instruction->set_block(block);
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[0]; }

    [[nodiscard]] std::string to_string() const override;
};
}

#endif

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
};

class Instruction : public User {
    std::weak_ptr<Block> block;
    Operator op;

public:
    Instruction(const std::string &name, const std::shared_ptr<Type::Type> &type, const Operator &op,
                const std::shared_ptr<Block> &block)
        : User{name, type}, block{block}, op{op} {
        const auto self = std::static_pointer_cast<Instruction>(shared_from_this());
        block->add_instruction(self);
    }

    ~Instruction() override = default;

    [[nodiscard]] std::shared_ptr<Block> get_block() const { return block.lock(); }

    [[nodiscard]] Operator get_op() const;

    [[nodiscard]] std::string to_string() const override = 0;
};

class Alloc final : public Instruction {
public:
    Alloc(const std::string &name, const std::shared_ptr<Type::Type> &type, const std::shared_ptr<Block> &block)
        : Instruction{name, std::make_shared<Type::Pointer>(type), Operator::ALLOC, block} {}

    [[nodiscard]] std::string to_string() const override;
};

class Load final : public Instruction {
public:
    Load(const std::string &name, const std::shared_ptr<Value> &addr,
         const std::shared_ptr<Block> &block)
        : Instruction{
            name, std::dynamic_pointer_cast<Type::Pointer>(addr->get_type())->get_contain_type(),
            Operator::LOAD, block
        } { add_operand(addr); }

    [[nodiscard]] std::shared_ptr<Value> get_addr() const { return operands_[0]; }

    [[nodiscard]] std::string to_string() const override;
};

class Store final : public Instruction {
public:
    Store(const std::shared_ptr<Value> &addr, const std::shared_ptr<Value> &value,
          const std::shared_ptr<Block> &block)
        : Instruction{"", Type::Void::void_, Operator::STORE, block} {
        add_operand(addr);
        add_operand(value);
    }

    [[nodiscard]] std::shared_ptr<Value> get_addr() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[1]; }

    [[nodiscard]] std::string to_string() const override;
};

class GetElementPtr final : public Instruction {
public:
    GetElementPtr(const std::string &name,
                  const std::shared_ptr<Value> &addr,
                  const std::shared_ptr<Value> &index,
                  const std::shared_ptr<Block> &block)
        : Instruction(name, calc_type_(addr), Operator::GEP, block) {
        if (!index->get_type()->is_int32()) { log_error("Index must be an integer 32"); }
        add_operand(addr);
        add_operand(index);
    }

    [[nodiscard]] std::shared_ptr<Value> get_addr() const { return operands_[0]; }
    [[nodiscard]] std::shared_ptr<Value> get_index() const { return operands_[1]; }

    [[nodiscard]] std::string to_string() const override;

private:
    static std::shared_ptr<Type::Type> calc_type_(const std::shared_ptr<Value> &addr);
};
}

#endif

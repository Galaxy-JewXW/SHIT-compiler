#ifndef INSTRUCTION_H
#define INSTRUCTION_H
#include "Const.h"
#include "Structure.h"
#include "Value.h"
#include "Utils/Log.h"

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
    RET,
    CALL,
    INTBINARY,
    FLOATBINARY,
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
        block->add_instruction(std::static_pointer_cast<Instruction>(shared_from_this()));
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
        } {
        if (!addr->get_type()->is_pointer()) { log_error("Address must be a pointer"); }
    }

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
        : Instruction{"", Type::Void::void_, Operator::STORE} {
        if (!addr->get_type()->is_pointer()) { log_error("Address must be a pointer"); }
        if (const auto contain_type = std::static_pointer_cast<Type::Pointer>(addr->get_type())
              ->get_contain_type(); *contain_type != *value->get_type()) {
            log_error("Address type: %s, value type: %s",
                      contain_type->to_string().c_str(),
                      value->get_type().get()->to_string().c_str());
        }
    }

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
        if (!addr->get_type()->is_pointer()) { log_error("Address must be a pointer"); }
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

    const Op op;

    Fcmp(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : Instruction(name, Type::Integer::i1, Operator::FCMP), op{op} {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) { log_error("Operands must be a float"); }
    }

    static std::shared_ptr<Value> create(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs,
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

    const Op op;

    Icmp(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : Instruction(name, Type::Integer::i1, Operator::ICMP), op{op} {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be an integer 32");
        }
    }

    static std::shared_ptr<Value> create(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs,
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

    static std::shared_ptr<Value> create(const std::shared_ptr<Value> &cond, const std::shared_ptr<Block> &true_block,
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
        return std::static_pointer_cast<Block>(operands_[1]);
    }

    [[nodiscard]] std::shared_ptr<Block> get_false_block() const {
        return std::static_pointer_cast<Block>(operands_[2]);
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
        return std::static_pointer_cast<Block>(operands_[0]);
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

class Call final : public Instruction {
    int const_string_index{-1};

public:
    explicit Call(const std::string &name, const std::shared_ptr<Function> &function,
                  const std::vector<std::shared_ptr<Value>> &params)
        : Instruction(name, function->get_type(), Operator::CALL) {
        if (function->get_type()->is_void() && !name.empty()) {
            log_error("Void function must not have a return value");
        }
    }

    explicit Call(const std::shared_ptr<Function> &function, const std::vector<std::shared_ptr<Value>> &params,
                  const int const_string_index = -1)
        : Instruction("", function->get_type(), Operator::CALL), const_string_index{const_string_index} {
        if (!function->get_type()->is_void()) {
            log_error("Non-Void function must have a return value");
        }
    }

    // 用于有返回值的函数
    static std::shared_ptr<Call> create(const std::string &name,
                                        const std::shared_ptr<Function> &function,
                                        const std::vector<std::shared_ptr<Value>> &params,
                                        const std::shared_ptr<Block> &block) {
        const auto instruction = std::make_shared<Call>(name, function, params);
        instruction->set_block(block);
        instruction->add_operand(function);
        for (const auto &param: params) {
            instruction->add_operand(param);
        }
        return instruction;
    }

    // 用于无返回值的函数
    // const_string_index用于存储常量字符串的索引
    static std::shared_ptr<Call> create(const std::shared_ptr<Function> &function,
                                        const std::vector<std::shared_ptr<Value>> &params,
                                        const std::shared_ptr<Block> &block,
                                        const int const_string_index = -1) {
        const auto instruction = std::make_shared<Call>(function, params, const_string_index);
        instruction->set_block(block);
        instruction->add_operand(function);
        for (const auto &param: params) {
            instruction->add_operand(param);
        }
        return instruction;
    }

    [[nodiscard]] std::shared_ptr<Value> get_function() const { return operands_[0]; }

    [[nodiscard]] std::vector<std::shared_ptr<Value>> get_params() const {
        if (operands_.size() <= 1) { return {}; }
        std::vector<std::shared_ptr<Value>> params;
        params.reserve(operands_.size() - 1);
        for (size_t i = 1; i < operands_.size(); ++i) { params.push_back(operands_[i]); }
        return params;
    }

    [[nodiscard]] int get_const_string_index() const { return const_string_index; }

    [[nodiscard]] std::string to_string() const override;
};

class Binary : public Instruction {
protected:
    Binary(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs,
           const Operator op)
        : Instruction(name, lhs->get_type(), op) {
        if (lhs->get_type() != rhs->get_type()) { log_error("Operands must have the same type"); }
    }

    ~Binary() override = default;

    [[nodiscard]] std::shared_ptr<Value> get_lhs() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Value> get_rhs() const { return operands_[1]; }

    [[nodiscard]] std::string to_string() const override = 0;
};

class IntBinary : public Binary {
public:
    enum class Op { ADD, SUB, MUL, DIV, MOD };

    const Op op;

    IntBinary(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs,
              const Op op)
        : Binary(name, lhs, rhs, Operator::INTBINARY), op{op} {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be int 32");
        }
    }

    [[nodiscard]] std::string to_string() const override = 0;
};

class FloatBinary : public Binary {
public:
    enum class Op { ADD, SUB, MUL, DIV, MOD };

    const Op op;

    FloatBinary(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs,
                const Op op)
        : Binary(name, lhs, rhs, Operator::FLOATBINARY), op{op} {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Operands must be float");
        }
    }

    [[nodiscard]] std::string to_string() const override = 0;
};

class Add final : public IntBinary {
public:
    explicit Add(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : IntBinary(name, lhs, rhs, Op::ADD) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be int 32");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstInt>(lhs),
                       right = std::dynamic_pointer_cast<ConstInt>(rhs);
            return std::make_shared<ConstInt>(*left + *right);
        }
        const auto instruction = std::make_shared<Add>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class Sub final : public IntBinary {
public:
    explicit Sub(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : IntBinary(name, lhs, rhs, Op::SUB) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be int 32");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstInt>(lhs),
                       right = std::dynamic_pointer_cast<ConstInt>(rhs);
            return std::make_shared<ConstInt>(*left - *right);
        }
        const auto instruction = std::make_shared<Sub>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class Mul final : public IntBinary {
public:
    explicit Mul(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : IntBinary(name, lhs, rhs, Op::MUL) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be int 32");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstInt>(lhs),
                       right = std::dynamic_pointer_cast<ConstInt>(rhs);
            return std::make_shared<ConstInt>(*left * *right);
        }
        const auto instruction = std::make_shared<Mul>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class Div final : public IntBinary {
public:
    explicit Div(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : IntBinary(name, lhs, rhs, Op::DIV) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be int 32");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstInt>(lhs),
                       right = std::dynamic_pointer_cast<ConstInt>(rhs);
            return std::make_shared<ConstInt>(*left / *right);
        }
        const auto instruction = std::make_shared<Div>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class Mod final : public IntBinary {
public:
    explicit Mod(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : IntBinary(name, lhs, rhs, Op::MOD) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be int 32");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstInt>(lhs),
                       right = std::dynamic_pointer_cast<ConstInt>(rhs);
            return std::make_shared<ConstInt>(*left % *right);
        }
        const auto instruction = std::make_shared<Mod>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class FAdd final : public FloatBinary {
public:
    explicit FAdd(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : FloatBinary(name, lhs, rhs, Op::ADD) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Operands must be float");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstFloat>(lhs),
                       right = std::dynamic_pointer_cast<ConstFloat>(rhs);
            return std::make_shared<ConstFloat>(*left + *right);
        }
        const auto instruction = std::make_shared<FAdd>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class FSub final : public FloatBinary {
public:
    explicit FSub(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : FloatBinary(name, lhs, rhs, Op::SUB) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Operands must be float");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstFloat>(lhs),
                       right = std::dynamic_pointer_cast<ConstFloat>(rhs);
            return std::make_shared<ConstFloat>(*left - *right);
        }
        const auto instruction = std::make_shared<FSub>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class FMul final : public FloatBinary {
public:
    explicit FMul(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : FloatBinary(name, lhs, rhs, Op::MUL) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Operands must be float");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstFloat>(lhs),
                       right = std::dynamic_pointer_cast<ConstFloat>(rhs);
            return std::make_shared<ConstFloat>(*left * *right);
        }
        const auto instruction = std::make_shared<FMul>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class FDiv final : public FloatBinary {
public:
    explicit FDiv(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : FloatBinary(name, lhs, rhs, Op::DIV) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Operands must be float");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstFloat>(lhs),
                       right = std::dynamic_pointer_cast<ConstFloat>(rhs);
            return std::make_shared<ConstFloat>(*left / *right);
        }
        const auto instruction = std::make_shared<FDiv>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};

class FMod final : public FloatBinary {
public:
    explicit FMod(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs)
        : FloatBinary(name, lhs, rhs, Op::MOD) {}

    static std::shared_ptr<Value> create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Operands must be float");
        }
        if (lhs->is_constant() && rhs->is_constant()) {
            const auto left = std::dynamic_pointer_cast<ConstFloat>(lhs),
                       right = std::dynamic_pointer_cast<ConstFloat>(rhs);
            return std::make_shared<ConstFloat>(*left % *right);
        }
        const auto instruction = std::make_shared<FMod>(name, lhs, rhs);
        instruction->set_block(block);
        instruction->add_operand(lhs);
        instruction->add_operand(rhs);
        return instruction;
    }

    [[nodiscard]] std::string to_string() const override;
};
}

#endif

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>

#include "Const.h"
#include "Interpreter.h"
#include "Structure.h"
#include "Utils/Log.h"
#include "Value.h"

namespace Mir {
class Block;
class Phi;

class FunctionCloneHelper;

enum class Operator {
    ALLOC,
    LOAD,
    STORE,
    GEP,
    BITCAST,
    FPTOSI,
    SITOFP,
    FCMP,
    ICMP,
    ZEXT,
    // terminator begins
    BRANCH,
    JUMP,
    RET,
    SWITCH,
    // terminator ends
    CALL,
    INTBINARY,
    FLOATBINARY,
    FLOATTERNARY,
    FNEG,
    PHI,
    SELECT,
    MOVE,
};

class Instruction : public User {
    std::weak_ptr<Block> block;
    Operator op;

protected:
    Instruction(const std::string &name, const std::shared_ptr<Type::Type> &type, const Operator &op) :
        User{name, type}, op{op} {}

public:
    [[nodiscard]] std::shared_ptr<Block> get_block() const { return block.lock(); }

    void set_block(const std::shared_ptr<Block> &block, const bool insert = true) {
        this->block = block;
        if (insert) [[likely]] {
            block->add_instruction(std::static_pointer_cast<Instruction>(shared_from_this()));
        }
    }

    [[nodiscard]] Operator get_op() const { return op; }

    [[nodiscard]] std::string to_string() const override = 0;

    virtual std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) {
        log_error("Not implemented");
    }

    virtual void fix_clone_info(const std::shared_ptr<Pass::LoopNodeClone> &clone_info);

    virtual void do_interpret(Interpreter *const interpreter) { Interpreter::abort(); }

    std::shared_ptr<Instruction> cloneinfo_to_block(const std::shared_ptr<Pass::LoopNodeClone> &clone_info,
                                                    const std::shared_ptr<Block> &block);

    virtual std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) { log_error("Not implemented"); }
};

class Alloc final : public Instruction {
public:
    Alloc(const std::string &name, const std::shared_ptr<Type::Type> &type) :
        Instruction{name, Type::Pointer::create(type), Operator::ALLOC} {}

    static std::shared_ptr<Alloc> create(const std::string &name, const std::shared_ptr<Type::Type> &type,
                                         const std::shared_ptr<Block> &block);

    std::shared_ptr<Instruction> clone_to_info(const std::shared_ptr<Block> &block) const {
        return create(name_, type_, block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;
};

class Load final : public Instruction {
public:
    Load(const std::string &name, const std::shared_ptr<Value> &addr) :
        Instruction{name, addr->get_type()->as<Type::Pointer>()->get_contain_type(), Operator::LOAD} {
        if (!addr->get_type()->is_pointer()) {
            log_error("Address must be a pointer");
        }
    }

    static std::shared_ptr<Load> create(const std::string &name, const std::shared_ptr<Value> &addr,
                                        const std::shared_ptr<Block> &block);

    std::shared_ptr<Instruction> clone_to_info(const std::shared_ptr<Block> &block) const {
        return create(name_, get_addr(), block);
    }

    [[nodiscard]] std::shared_ptr<Value> get_addr() const { return operands_[0]; }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;
};

class Store final : public Instruction {
public:
    Store(const std::shared_ptr<Value> &addr, const std::shared_ptr<Value> &value) :
        Instruction{"", Type::Void::void_, Operator::STORE} {
        if (!addr->get_type()->is_pointer()) {
            log_error("Address must be a pointer");
        }
        if (const auto contain_type = std::static_pointer_cast<Type::Pointer>(addr->get_type())->get_contain_type();
            *contain_type != *value->get_type()) {
            log_error("Address type: %s, value type: %s", contain_type->to_string().c_str(),
                      value->get_type().get()->to_string().c_str());
        }
    }

    static std::shared_ptr<Store> create(const std::shared_ptr<Value> &addr, const std::shared_ptr<Value> &value,
                                         const std::shared_ptr<Block> &block);

    std::shared_ptr<Instruction> clone_to_info(const std::shared_ptr<Block> &block) const {
        return create(get_addr(), get_value(), block);
    }

    [[nodiscard]] std::shared_ptr<Value> get_addr() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[1]; }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;
};

class GetElementPtr final : public Instruction {
public:
    GetElementPtr(const std::string &name, const std::shared_ptr<Value> &addr,
                  const std::vector<std::shared_ptr<Value>> &indexes) :
        Instruction(name, calc_type_(addr, indexes), Operator::GEP) {
        if (!addr->get_type()->is_pointer()) {
            log_error("Address must be a pointer");
        }
    }

    static std::shared_ptr<GetElementPtr> create(const std::string &name, const std::shared_ptr<Value> &addr,
                                                 const std::vector<std::shared_ptr<Value>> &indexes,
                                                 const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_addr() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Value> get_index() const { return operands_.back(); }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        std::vector<std::shared_ptr<Value>> indexes;
        for (size_t i = 1; i < operands_.size(); ++i) {
            indexes.push_back(operands_[i]);
        }
        return create(name_, get_addr(), indexes, block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

private:
    static std::shared_ptr<Type::Type> calc_type_(const std::shared_ptr<Value> &addr,
                                                  const std::vector<std::shared_ptr<Value>> &indexes);
};

class BitCast final : public Instruction {
public:
    BitCast(const std::string &name, const std::shared_ptr<Value> &value,
            const std::shared_ptr<Type::Type> &target_type) :
        Instruction(name, target_type, Operator::BITCAST) {
        if (value->get_type()->is_void() || value->get_type()->is_label() || value->get_name().empty()) {
            log_error("Instruction must have a return value");
        }
    }

    static std::shared_ptr<BitCast> create(const std::string &name, const std::shared_ptr<Value> &value,
                                           const std::shared_ptr<Type::Type> &target_type,
                                           const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[0]; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(name_, get_value(), get_type(), block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;
};

class Fptosi final : public Instruction {
public:
    Fptosi(const std::string &name, const std::shared_ptr<Value> &value) :
        Instruction(name, Type::Integer::i32, Operator::FPTOSI) {
        if (!value->get_type()->is_float()) {
            log_error("Value must be a float");
        }
    }

    static std::shared_ptr<Fptosi> create(const std::string &name, const std::shared_ptr<Value> &value,
                                          const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[0]; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(name_, get_value(), block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;
};

class Sitofp final : public Instruction {
public:
    Sitofp(const std::string &name, const std::shared_ptr<Value> &value) :
        Instruction(name, Type::Float::f32, Operator::SITOFP) {
        if (!value->get_type()->is_int32()) {
            log_error("Value must be an integer 32");
        }
    }

    static std::shared_ptr<Sitofp> create(const std::string &name, const std::shared_ptr<Value> &value,
                                          const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[0]; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(name_, get_value(), block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;
};

class Fcmp final : public Instruction {
public:
    enum class Op { EQ, NE, GT, LT, GE, LE };

    Op op;

    Fcmp(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs) :
        Instruction(name, Type::Integer::i1, Operator::FCMP), op{op} {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Operands must be a float");
        }
    }

    static Op swap_op(const Op op) {
        switch (op) {
            case Op::GT:
                return Op::LT;
            case Op::LT:
                return Op::GT;
            case Op::GE:
                return Op::LE;
            case Op::LE:
                return Op::GE;
            default:
                return op;
        }
    }

    void reverse_op() {
        this->op = swap_op(this->op);
        std::swap(operands_[0], operands_[1]);
    }

    static std::shared_ptr<Fcmp> create(const std::string &name, Op op, std::shared_ptr<Value> lhs,
                                        std::shared_ptr<Value> rhs, const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_lhs() const { return operands_[0]; }
    [[nodiscard]] std::shared_ptr<Value> get_rhs() const { return operands_[1]; }

    [[nodiscard]] Op fcmp_op() const { return op; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(name_, fcmp_op(), get_lhs(), get_rhs(), block); // Fcmp 用 fcmp_op()
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;
};

class Icmp final : public Instruction {
public:
    enum class Op { EQ, NE, GT, LT, GE, LE };

    Op op;

    Icmp(const std::string &name, const Op op, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs) :
        Instruction(name, Type::Integer::i1, Operator::ICMP), op{op} {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be an integer 32");
        }
    }

    static Op swap_op(const Op op) {
        switch (op) {
            case Op::GT:
                return Op::LT;
            case Op::LT:
                return Op::GT;
            case Op::GE:
                return Op::LE;
            case Op::LE:
                return Op::GE;
            default:
                return op;
        }
    }

    static Op inverse_op(const Op op) {
        switch (op) {
            case Op::EQ:
                return Op::NE;
            case Op::NE:
                return Op::EQ;
            case Op::GT:
                return Op::LE;
            case Op::LT:
                return Op::GE;
            case Op::GE:
                return Op::LT;
            case Op::LE:
                return Op::GT;
        }
        return op;
    }

    void reverse_op() {
        this->op = swap_op(this->op);
        std::swap(operands_[0], operands_[1]);
    }

    static std::shared_ptr<Icmp> create(const std::string &name, Op op, std::shared_ptr<Value> lhs,
                                        std::shared_ptr<Value> rhs, const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_lhs() const { return operands_[0]; }
    [[nodiscard]] std::shared_ptr<Value> get_rhs() const { return operands_[1]; }

    [[nodiscard]] Op icmp_op() const { return op; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(name_, icmp_op(), get_lhs(), get_rhs(), block); // Fcmp 用 fcmp_op()
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;
};

class Zext final : public Instruction {
public:
    Zext(const std::string &name, const std::shared_ptr<Value> &value) :
        Instruction(name, Type::Integer::i32, Operator::ZEXT) {
        if (!value->get_type()->is_int1()) {
            log_error("Value must be an integer 1");
        }
    }

    static std::shared_ptr<Zext> create(const std::string &name, const std::shared_ptr<Value> &value,
                                        const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[0]; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(name_, get_value(), block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;
};

class Terminator : public Instruction {
protected:
    Terminator(const std::shared_ptr<Type::Type> &type, const Operator op) :
        Instruction("", type, op) {}
};

class Jump final : public Terminator {
public:
    explicit Jump(const std::shared_ptr<Block> &) :
        Terminator(Type::Label::label, Operator::JUMP) {}

    static std::shared_ptr<Jump> create(const std::shared_ptr<Block> &target_block,
                                        const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Block> get_target_block() const {
        return std::static_pointer_cast<Block>(operands_[0]);
    }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(get_target_block(), block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;
};

class Branch final : public Terminator {
public:
    Branch(const std::shared_ptr<Value> &cond, const std::shared_ptr<Block> &, const std::shared_ptr<Block> &) :
        Terminator(Type::Label::label, Operator::BRANCH) {
        if (!cond->get_type()->is_int1()) {
            log_error("Cond must be an integer 1");
        }
    }

    static std::shared_ptr<Branch> create(const std::shared_ptr<Value> &cond, const std::shared_ptr<Block> &true_block,
                                          const std::shared_ptr<Block> &false_block,
                                          const std::shared_ptr<Block> &block);

    void swap() { std::swap(operands_[0], operands_[1]); }

    [[nodiscard]] std::shared_ptr<Value> get_cond() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Block> get_true_block() const {
        return std::static_pointer_cast<Block>(operands_[1]);
    }

    [[nodiscard]] std::shared_ptr<Block> get_false_block() const {
        return std::static_pointer_cast<Block>(operands_[2]);
    }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(get_cond(), get_true_block(), get_false_block(), block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;
};

class Ret final : public Terminator {
public:
    explicit Ret(const std::shared_ptr<Value> &value) :
        Terminator(Type::Void::void_, Operator::RET) {
        if (value->get_type()->is_void()) {
            log_error("Value must not be void");
        }
    }

    explicit Ret() :
        Terminator(Type::Void::void_, Operator::RET) {}

    static std::shared_ptr<Ret> create(const std::shared_ptr<Value> &value, const std::shared_ptr<Block> &block);

    static std::shared_ptr<Ret> create(const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_value() const {
        if (operands_.empty()) [[unlikely]] {
            return nullptr;
        }
        return operands_[0];
    }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(get_value(), block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;
};

class Switch final : public Terminator {
public:
    Switch(const std::shared_ptr<Value> &base, const std::shared_ptr<Value> &) :
        Terminator(Type::Void::void_, Operator::SWITCH) {
        if (!base->get_type()->is_integer() && !base->get_type()->is_float()) {
            log_error("Not supported");
        }
    }

    static std::shared_ptr<Switch> create(const std::shared_ptr<Value> &base,
                                          const std::shared_ptr<Block> &default_block,
                                          const std::shared_ptr<Block> &block);

    std::shared_ptr<Value> get_base() const { return operands_[0]; }

    std::shared_ptr<Block> get_default_block() const { return operands_[1]->as<Block>(); }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;

    const std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Block>> &cases() const { return cases_table; }

    std::optional<std::shared_ptr<Block>> get_case(const std::shared_ptr<Value> &value) const {
        if (cases_table.find(value) != cases_table.end()) {
            return std::make_optional(cases_table.at(value));
        }
        return std::nullopt;
    }

    void set_case(const std::shared_ptr<Const> &value, const std::shared_ptr<Block> &block);

    void set_case(const std::pair<const std::shared_ptr<Const>, std::shared_ptr<Block>> &pair);

    void remove_case(const std::shared_ptr<Const> &value);

    void modify_operand(const std::shared_ptr<Value> &old_value, const std::shared_ptr<Value> &new_value) override;

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        auto sw = create(get_base(), get_default_block(), block);
        for (const auto &[val, blk]: cases()) {
            auto const_val = val->as<Const>();
            sw->set_case(const_val, blk);
        }
        return sw;
    }

private:
    // 跳转表
    std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Block>> cases_table;
};

class Call final : public Instruction {
    int const_string_index{-1};

    bool is_tail_call_{false};

public:
    explicit Call(const std::string &name, const std::shared_ptr<Function> &function,
                  const std::vector<std::shared_ptr<Value>> &) :
        Instruction(name, function->get_type(), Operator::CALL) {
        if (function->get_type()->is_void() && !name.empty()) {
            log_error("Void function must not have a return value");
        }
    }

    explicit Call(const std::shared_ptr<Function> &function, const std::vector<std::shared_ptr<Value>> &,
                  const int const_string_index = -1) :
        Instruction("", function->get_type(), Operator::CALL), const_string_index{const_string_index} {
        if (!function->get_type()->is_void()) {
            log_error("Non-Void function must have a return value");
        }
    }

    // 用于有返回值的函数
    static std::shared_ptr<Call> create(const std::string &name, const std::shared_ptr<Function> &function,
                                        const std::vector<std::shared_ptr<Value>> &params,
                                        const std::shared_ptr<Block> &block);

    // 用于无返回值的函数
    // const_string_index用于存储常量字符串的索引
    static std::shared_ptr<Call> create(const std::shared_ptr<Function> &function,
                                        const std::vector<std::shared_ptr<Value>> &params,
                                        const std::shared_ptr<Block> &block, int const_string_index = -1);

    [[nodiscard]] std::shared_ptr<Value> get_function() const { return operands_[0]; }

    [[nodiscard]] std::vector<std::shared_ptr<Value>> get_params() const {
        if (operands_.size() <= 1) {
            return {};
        }
        std::vector<std::shared_ptr<Value>> params;
        params.reserve(operands_.size() - 1);
        for (size_t i = 1; i < operands_.size(); ++i) {
            params.push_back(operands_[i]);
        }
        return params;
    }

    [[nodiscard]] int get_const_string_index() const { return const_string_index; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        if (get_name().empty())
            return create(get_function()->as<Function>(), get_params(), block, get_const_string_index());
        else
            return create(get_name(), get_function()->as<Function>(), get_params(), block);
    }

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

    void do_interpret(Interpreter *interpreter) override;

    [[nodiscard]] bool is_tail_call() const { return is_tail_call_; }

    void set_tail_call(const bool flag = true) { is_tail_call_ = flag; }
};

class Binary : public Instruction {
protected:
    Binary(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs,
           const Operator op) :
        Instruction(name, lhs->get_type(), op) {
        if (lhs->get_type() != rhs->get_type()) {
            log_error("Operands must have the same type");
        }
    }

public:
    [[nodiscard]] std::shared_ptr<Value> get_lhs() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Value> get_rhs() const { return operands_[1]; }

    void swap_operands() { std::swap(operands_[0], operands_[1]); }

    [[nodiscard]] std::string to_string() const override = 0;

    // 满足交换律
    virtual bool is_commutative() const = 0;

    // 满足结合律
    virtual bool is_associative() const = 0;

    [[nodiscard]]
    virtual std::shared_ptr<Instruction> clone_exact() {
        log_error("Not implemented");
    }
};

class IntBinary : public Binary {
public:
    enum class Op { ADD, SUB, MUL, DIV, MOD, AND, OR, XOR, SMAX, SMIN };

    const Op op;

    IntBinary(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs,
              const Op op) :
        Binary(name, lhs, rhs, Operator::INTBINARY), op{op} {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Operands must be int 32");
        }
    }

    [[nodiscard]] Op intbinary_op() const { return op; }

    [[nodiscard]] std::string to_string() const override = 0;

    bool is_commutative() const override {
        switch (op) {
            case Op::ADD:
            case Op::MUL:
            case Op::AND:
            case Op::OR:
            case Op::XOR:
            case Op::SMAX:
            case Op::SMIN:
                return true;
            default:
                return false;
        }
    }

    bool is_associative() const override {
        switch (op) {
            case Op::ADD:
            case Op::MUL:
            case Op::AND:
            case Op::OR:
            case Op::XOR:
            case Op::SMAX:
            case Op::SMIN:
                return true;
            default:
                return false;
        }
    }

    static bool is_associative_op(const Op op) {
        switch (op) {
            case Op::ADD:
            case Op::MUL:
            case Op::AND:
            case Op::OR:
            case Op::XOR:
            case Op::SMAX:
            case Op::SMIN:
                return true;
            default:
                return false;
        }
    }
};

class FloatBinary : public Binary {
public:
    enum class Op { ADD, SUB, MUL, DIV, MOD, SMAX, SMIN };

    const Op op;

    FloatBinary(const std::string &name, const std::shared_ptr<Value> &lhs, const std::shared_ptr<Value> &rhs,
                const Op op) :
        Binary(name, lhs, rhs, Operator::FLOATBINARY), op{op} {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Operands must be float");
        }
    }

    [[nodiscard]] Op floatbinary_op() const { return op; }

    [[nodiscard]] std::string to_string() const override = 0;

    bool is_commutative() const override {
        switch (op) {
            case Op::ADD:
            case Op::MUL:
            case Op::SMAX:
            case Op::SMIN:
                return true;
            default:
                return false;
        }
    }

    bool is_associative() const override {
        switch (op) {
            case Op::ADD:
            case Op::MUL:
            case Op::SMAX:
            case Op::SMIN:
                return true;
            default:
                return false;
        }
    }
};

class FloatTernary : public Instruction {
public:
    enum class Op { FMADD, FMSUB, FNMADD, FNMSUB };

    const Op op;

    FloatTernary(const std::string &name, const std::shared_ptr<Value> &x, const std::shared_ptr<Value> &y,
                 const std::shared_ptr<Value> &z, const Op op) :
        Instruction(name, Type::Float::f32, Operator::FLOATTERNARY), op{op} {
        if (!x->get_type()->is_float() || !y->get_type()->is_float() || !z->get_type()->is_float()) {
            log_error("Operands must be float");
        }
    }

    [[nodiscard]] std::shared_ptr<Value> get_x() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Value> get_y() const { return operands_[1]; }

    [[nodiscard]] std::shared_ptr<Value> get_z() const { return operands_[2]; }

    [[nodiscard]] Op floatternary_op() const { return op; }

    [[nodiscard]] std::string to_string() const override = 0;
};

#define INTBINARY_DECLARE(Class, op)                                                                                   \
    class Class final : public IntBinary {                                                                             \
    public:                                                                                                            \
        explicit Class(const std::string &name, const std::shared_ptr<Value> &lhs,                                     \
                       const std::shared_ptr<Value> &rhs) : IntBinary(name, lhs, rhs, op) {}                           \
        static std::shared_ptr<Class> create(const std::string &name, const std::shared_ptr<Value> &lhs,               \
                                             const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block);  \
        std::shared_ptr<Instruction> clone_exact() override {                                                          \
            return create(get_name(), get_lhs(), get_rhs(), get_block());                                              \
        }                                                                                                              \
        std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {                    \
            return create(get_name(), get_lhs(), get_rhs(), block);                                                    \
        }                                                                                                              \
        [[nodiscard]] std::string to_string() const override;                                                          \
        void do_interpret(Interpreter *interpreter) override;                                                          \
        [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;                        \
    };

#define FLOATBINARY_DECLARE(Class, op)                                                                                 \
    class Class final : public FloatBinary {                                                                           \
    public:                                                                                                            \
        explicit Class(const std::string &name, const std::shared_ptr<Value> &lhs,                                     \
                       const std::shared_ptr<Value> &rhs) : FloatBinary(name, lhs, rhs, op) {}                         \
        static std::shared_ptr<Class> create(const std::string &name, const std::shared_ptr<Value> &lhs,               \
                                             const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block);  \
        std::shared_ptr<Instruction> clone_exact() override {                                                          \
            return create(get_name(), get_lhs(), get_rhs(), get_block());                                              \
        }                                                                                                              \
        std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {                    \
            return create(get_name(), get_lhs(), get_rhs(), block);                                                    \
        }                                                                                                              \
        [[nodiscard]] std::string to_string() const override;                                                          \
        void do_interpret(Interpreter *interpreter) override;                                                          \
        [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;                        \
    };

#define FLOATTENARY_DECLARE(Class, op)                                                                                 \
    class Class final : public FloatTernary {                                                                          \
    public:                                                                                                            \
        explicit Class(const std::string &name, const std::shared_ptr<Value> &x, const std::shared_ptr<Value> &y,      \
                       const std::shared_ptr<Value> &z) : FloatTernary(name, x, y, z, op) {}                           \
        static std::shared_ptr<Class> create(const std::string &name, const std::shared_ptr<Value> &x,                 \
                                             const std::shared_ptr<Value> &y, const std::shared_ptr<Value> &z,         \
                                             const std::shared_ptr<Block> &block);                                     \
        std::shared_ptr<Instruction> clone_exact() {                                                                   \
            return create(get_name(), operands_[0], operands_[1], operands_[2], get_block());                          \
        }                                                                                                              \
        std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {                    \
            return create(get_name(), operands_[0], operands_[1], operands_[2], block);                                \
        }                                                                                                              \
        [[nodiscard]] std::string to_string() const override;                                                          \
        void do_interpret(Interpreter *interpreter) override;                                                          \
        [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;                        \
    };

INTBINARY_DECLARE(Add, Op::ADD)

INTBINARY_DECLARE(Sub, Op::SUB)

INTBINARY_DECLARE(Mul, Op::MUL)

INTBINARY_DECLARE(Div, Op::DIV)

INTBINARY_DECLARE(Mod, Op::MOD)

INTBINARY_DECLARE(And, Op::AND)

INTBINARY_DECLARE(Or, Op::OR)

INTBINARY_DECLARE(Xor, Op::XOR)

INTBINARY_DECLARE(Smax, Op::SMAX)

INTBINARY_DECLARE(Smin, Op::SMIN)

FLOATBINARY_DECLARE(FAdd, Op::ADD)

FLOATBINARY_DECLARE(FSub, Op::SUB)

FLOATBINARY_DECLARE(FMul, Op::MUL)

FLOATBINARY_DECLARE(FDiv, Op::DIV)

FLOATBINARY_DECLARE(FMod, Op::MOD)

FLOATBINARY_DECLARE(FSmax, Op::SMAX)

FLOATBINARY_DECLARE(FSmin, Op::SMIN)

FLOATTENARY_DECLARE(FMadd, Op::FMADD)

FLOATTENARY_DECLARE(FMsub, Op::FMSUB)

FLOATTENARY_DECLARE(FNmadd, Op::FNMADD)

FLOATTENARY_DECLARE(FNmsub, Op::FNMSUB)

#undef INTBINARY_DECLARE
#undef FLOATBINARY_DECLARE
#undef FLOATTENARY_DECLARE

class FNeg final : public Instruction {
public:
    explicit FNeg(const std::string &name, const std::shared_ptr<Value> &value) :
        Instruction{name, value->get_type(), Operator::FNEG} {
        if (!value->get_type()->is_float()) {
            log_error("value should be float");
        }
    }

    static std::shared_ptr<FNeg> create(const std::string &name, const std::shared_ptr<Value> &value,
                                        const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_value() const { return operands_[0]; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(name_, get_value(), block);
    }

    [[nodiscard]] std::string to_string() const override;

    void do_interpret(Interpreter *interpreter) override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;
};

class Phi final : public Instruction {
public:
    using Optional_Values = std::unordered_map<std::shared_ptr<Block>, std::shared_ptr<Value>>;

    explicit Phi(const std::string &name, const std::shared_ptr<Type::Type> &type, Optional_Values optional_values) :
        Instruction{name, type, Operator::PHI}, optional_values{std::move(optional_values)} {}

    static std::shared_ptr<Phi> create(const std::string &name, const std::shared_ptr<Type::Type> &type,
                                       const std::shared_ptr<Block> &block, const Optional_Values &optional_values);

    [[nodiscard]] std::string to_string() const override;

    [[nodiscard]] const Optional_Values &get_optional_values() { return optional_values; }

    [[nodiscard]] std::shared_ptr<Value> get_value_by_block(const std::shared_ptr<Block> &block);

    void set_optional_value(const std::shared_ptr<Block> &block, const std::shared_ptr<Value> &optional_value);

    void remove_optional_value(const std::shared_ptr<Block> &block);

    void modify_operand(const std::shared_ptr<Value> &old_value, const std::shared_ptr<Value> &new_value) override;

    [[deprecated]] std::shared_ptr<Block> find_optional_block(const std::shared_ptr<Value> &value);

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(name_, type_, block, get_optional_values());
    }

    void fix_clone_info(const std::shared_ptr<Pass::LoopNodeClone> &clone_info) override;

    void do_interpret(Interpreter *interpreter) override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;

private:
    Optional_Values optional_values;
};

class Select final : public Instruction {
public:
    explicit Select(const std::string &name, const std::shared_ptr<Value> &condition,
                    const std::shared_ptr<Value> &true_value, const std::shared_ptr<Value> &false_value) :
        Instruction(name, true_value->get_type(), Operator::SELECT) {
        if (*true_value->get_type() != *false_value->get_type()) {
            log_error("lhs and rhs should be same type");
        }
        if (condition->get_type() != Type::Integer::i1) {
            log_error("condition should be an i1");
        }
    }

    static std::shared_ptr<Select> create(const std::string &name, const std::shared_ptr<Value> &condition,
                                          const std::shared_ptr<Value> &true_value,
                                          const std::shared_ptr<Value> &false_value,
                                          const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_cond() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Value> get_true_value() const { return operands_[1]; }

    [[nodiscard]] std::shared_ptr<Value> get_false_value() const { return operands_[2]; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        return create(name_, get_cond(), get_true_value(), get_false_value(), block);
    }

    std::string to_string() const override;

    void do_interpret(Interpreter *interpreter) override;

    [[nodiscard]] std::shared_ptr<Instruction> clone(FunctionCloneHelper &helper) override;
};

class Move final : public Instruction {
public:
    Move(const std::shared_ptr<Value> &to_value, const std::shared_ptr<Value> &from_value) :
        Instruction{"", Type::Void::void_, Operator::MOVE} {
        if (*to_value->get_type() != *from_value->get_type()) [[unlikely]] {
            log_error("Type mismatch");
        }
    }

    static std::shared_ptr<Move> create(const std::shared_ptr<Value> &to_value,
                                        const std::shared_ptr<Value> &from_value,
                                        const std::shared_ptr<Block> &block);

    [[nodiscard]] std::shared_ptr<Value> get_to_value() const { return operands_[0]; }

    [[nodiscard]] std::shared_ptr<Value> get_from_value() const { return operands_[1]; }

    std::shared_ptr<Instruction> clone_to_block(const std::shared_ptr<Block> &block) override {
        log_error("Not implemented");
    }

    std::string to_string() const override;
};

template<typename T, typename... Ts>
std::shared_ptr<T> make_noinsert_instruction(Ts &&... args) {
    return T::create(std::forward<Ts>(args)..., nullptr);
}
} // namespace Mir

#endif

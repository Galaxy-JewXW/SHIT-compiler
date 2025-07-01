#include "Mir/Instruction.h"

namespace Mir {
std::shared_ptr<Alloc> Alloc::create(const std::string &name, const std::shared_ptr<Type::Type> &type,
                                     const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Alloc>(name, type);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    return instruction;
}

std::shared_ptr<Load> Load::create(const std::string &name, const std::shared_ptr<Value> &addr,
                                   const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Load>(name, addr);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(addr);
    return instruction;
}

std::shared_ptr<Store> Store::create(const std::shared_ptr<Value> &addr,
                                     const std::shared_ptr<Value> &value,
                                     const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Store>(addr, value);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(addr);
    instruction->add_operand(value);
    return instruction;
}

/**
 * 计算addr经过一次indexing得到的指针类型
 * 1. 如果addr是数组指针，则返回其元素的指针
 *   - e.g. %4 = getelementptr inbounds [2 x [2 x i32]], [2 x [2 x i32]]* %2, i32 0, i32 1
 *     - addr的类型为[2 x [2 x i32]]*，此时默认进行一次索引将其转化为[2 x [2 x i32]]，
 *     - 第二次索引将其转换为执行第二个子数组的指针，类型为[2 x i32]*
 * 2. 如果addr是指向基本类型(int 或 float)的指针，则直接进行偏移，类型不发生变化
 *   - %1 = getelementptr inbounds i32, i32* %0, i32 1
 *     - %0 和 %1 的类型均为i32*
 */
std::shared_ptr<Type::Type> GetElementPtr::calc_type_(const std::shared_ptr<Value> &addr,
                                                      const std::vector<std::shared_ptr<Value>> &indexes) {
    const auto type = addr->get_type();
    const auto ptr_type = std::dynamic_pointer_cast<Type::Pointer>(type);
    if (!ptr_type) {
        log_error("First operand of getelementptr must be a pointer type");
    }
    if (indexes.size() == 1) {
        return ptr_type;
    }
    if (indexes.size() >= 2) {
        for (size_t i = 0; i < indexes.size() - 1; ++i) {
            if (const auto constant_zero = std::dynamic_pointer_cast<ConstInt>(indexes[i]);
                constant_zero == nullptr) {
                log_error("Index should be constant zero");
            } else if (**constant_zero != 0) {
                log_error("Index should be zero");
            }
        }
        auto current = ptr_type->get_contain_type();
        for (size_t i = 1; i < indexes.size(); ++i) {
            if (!current->is_array()) {
                log_error("Indexing on non-array type");
            }
            current = current->as<Type::Array>()->get_element_type();
        }
        return Type::Pointer::create(current);
    }
    log_error("Invalid indexes size %d", indexes.size());
}

std::shared_ptr<Value> GetElementPtr::create(const std::string &name,
                                             const std::shared_ptr<Value> &addr,
                                             const std::vector<std::shared_ptr<Value>> &indexes,
                                             const std::shared_ptr<Block> &block) {
    if (indexes.size() == 1) {
        if (const auto &idx = indexes[0]; idx->is_constant()) {
            const auto constant_idx = std::dynamic_pointer_cast<ConstInt>(idx);
            if (constant_idx == nullptr) {
                log_error("Index should be constant");
            }
            if (**constant_idx == 0) {
                return addr;
            }
        }
    }
    const auto instruction = std::make_shared<GetElementPtr>(name, addr, indexes);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    if (const auto type = addr->get_type(); !type->is_pointer()) {
        log_error("First operand must be a pointer type");
    }
    instruction->add_operand(addr);
    for (const auto &index: indexes) {
        instruction->add_operand(index);
    }
    return instruction;
}

std::shared_ptr<BitCast> BitCast::create(const std::string &name, const std::shared_ptr<Value> &value,
                                         const std::shared_ptr<Type::Type> &target_type,
                                         const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<BitCast>(name, value, target_type);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

std::shared_ptr<Fptosi> Fptosi::create(const std::string &name, const std::shared_ptr<Value> &value,
                                       const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Fptosi>(name, value);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

std::shared_ptr<Sitofp> Sitofp::create(const std::string &name, const std::shared_ptr<Value> &value,
                                       const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Sitofp>(name, value);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

std::shared_ptr<Value> Fcmp::create(const std::string &name, Op op, std::shared_ptr<Value> lhs,
                                    std::shared_ptr<Value> rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) { log_error("Operands must be a float"); }
    if (lhs->is_constant() && rhs->is_constant()) {
        const auto left = std::dynamic_pointer_cast<ConstFloat>(lhs),
                   right = std::dynamic_pointer_cast<ConstFloat>(rhs);
        switch (op) {
            case Op::EQ: return ConstBool::create(*left == *right);
            case Op::NE: return ConstBool::create(*left != *right);
            case Op::GT: return ConstBool::create(*left > *right);
            case Op::LT: return ConstBool::create(*left < *right);
            case Op::GE: return ConstBool::create(*left >= *right);
            case Op::LE: return ConstBool::create(*left <= *right);
            default: break;
        }
    }
    if (lhs->is_constant() && !rhs->is_constant()) {
        std::swap(lhs, rhs);
        op = swap_op(op);
    }
    const auto instruction = std::make_shared<Fcmp>(name, op, lhs, rhs);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<Value> Icmp::create(const std::string &name, Op op, std::shared_ptr<Value> lhs,
                                    std::shared_ptr<Value> rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
        log_error("Operands must be an integer 32");
    }
    if (lhs->is_constant() && rhs->is_constant()) {
        const auto left = std::dynamic_pointer_cast<ConstInt>(lhs),
                   right = std::dynamic_pointer_cast<ConstInt>(rhs);
        switch (op) {
            case Op::EQ: return ConstBool::create(*left == *right);
            case Op::NE: return ConstBool::create(*left != *right);
            case Op::GT: return ConstBool::create(*left > *right);
            case Op::LT: return ConstBool::create(*left < *right);
            case Op::GE: return ConstBool::create(*left >= *right);
            case Op::LE: return ConstBool::create(*left <= *right);
            default: break;
        }
    }
    if (lhs->is_constant() && !rhs->is_constant()) {
        std::swap(lhs, rhs);
        op = swap_op(op);
    }
    const auto instruction = std::make_shared<Icmp>(name, op, lhs, rhs);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<Zext> Zext::create(const std::string &name, const std::shared_ptr<Value> &value,
                                   const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Zext>(name, value);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

std::shared_ptr<Jump> Jump::create(const std::shared_ptr<Block> &target_block,
                                   const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Jump>(target_block);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(target_block);
    return instruction;
}

std::shared_ptr<Value> Branch::create(const std::shared_ptr<Value> &cond, const std::shared_ptr<Block> &true_block,
                                      const std::shared_ptr<Block> &false_block,
                                      const std::shared_ptr<Block> &block) {
    if (!cond->get_type()->is_int1()) {
        log_error("Cond must be an integer 1");
    }
    if (cond->is_constant()) {
        if (const auto value = cond->as<ConstBool>()->get<int>(); value == 1) {
            return Jump::create(true_block, block);
        }
        return Jump::create(false_block, block);
    }
    const auto instruction = std::make_shared<Branch>(cond, true_block, false_block);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(cond);
    instruction->add_operand(true_block);
    instruction->add_operand(false_block);
    return instruction;
}

std::shared_ptr<Ret> Ret::create(const std::shared_ptr<Value> &value, const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Ret>(value);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

std::shared_ptr<Ret> Ret::create(const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Ret>();
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    return instruction;
}

std::shared_ptr<Call> Call::create(const std::string &name,
                                   const std::shared_ptr<Function> &function,
                                   const std::vector<std::shared_ptr<Value>> &params,
                                   const std::shared_ptr<Block> &block) {
    if (function->get_return_type()->is_void()) {
        log_error("Void function must not have a return value");
    }
    const auto instruction = std::make_shared<Call>(name, function, params);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(function);
    const auto &func_arguments = function->get_arguments();
    if (params.size() != func_arguments.size()) {
        log_error("Expected %zu arguments, got %zu", func_arguments.size(), params.size());
    }
    for (size_t i = 0; i < params.size(); ++i) {
        const auto &param_received = params[i];
        if (*param_received->get_type() != *func_arguments[i]->get_type()) {
            log_error("Expected argument type %s, got %s", func_arguments[i]->get_type()->to_string().c_str(),
                      param_received->get_type()->to_string().c_str());
        }
        instruction->add_operand(param_received);
    }
    return instruction;
}

std::shared_ptr<Call> Call::create(const std::shared_ptr<Function> &function,
                                   const std::vector<std::shared_ptr<Value>> &params,
                                   const std::shared_ptr<Block> &block,
                                   const int const_string_index) {
    if (!function->get_return_type()->is_void()) {
        log_error("Non-Void function must have a return value");
    }
    const auto instruction = std::make_shared<Call>(function, params, const_string_index);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(function);
    const auto &func_arguments = function->get_arguments();
    if (params.size() != func_arguments.size() && function->get_name() != "putf") {
        log_error("Expected %zu arguments, got %zu", func_arguments.size(), params.size());
    }
    for (size_t i = 0; i < params.size(); ++i) {
        const auto &param_received = params[i];
        if (function->get_name() != "putf") {
            if (*param_received->get_type() != *func_arguments[i]->get_type()) {
                log_error("Expected argument type %s, got %s", func_arguments[i]->get_type()->to_string().c_str(),
                          param_received->get_type()->to_string().c_str());
            }
        }
        instruction->add_operand(param_received);
    }
    return instruction;
}

#define CREATE_BINARY(Type, TypeCheck)                                         \
std::shared_ptr<Type> Type::create(const std::string &name,                    \
                                 const std::shared_ptr<Value> &lhs,            \
                                 const std::shared_ptr<Value> &rhs,            \
                                 const std::shared_ptr<Block> &block) {        \
    if (!lhs->get_type()->TypeCheck() || !rhs->get_type()->TypeCheck()) {      \
        log_error("Operands does not fit %s", #TypeCheck);                     \
    }                                                                          \
    const auto instruction = std::make_shared<Type>(name, lhs, rhs);           \
    if (block != nullptr) [[likely]] {                                         \
        instruction->set_block(block);                                         \
    }                                                                          \
    instruction->add_operand(lhs);                                             \
    instruction->add_operand(rhs);                                             \
    return instruction;                                                        \
}

#define CREATE_TERNARY(Type, TypeCheck)                                        \
std::shared_ptr<Type> Type::create(                                            \
    const std::string &name, const std::shared_ptr<Value> &x,                  \
    const std::shared_ptr<Value> &y, const std::shared_ptr<Value> &z,          \
    const std::shared_ptr<Block> &block) {                                     \
    if (!x->get_type()->TypeCheck() || !y->get_type()->TypeCheck() ||          \
        !z->get_type()->TypeCheck()) {                                         \
        log_error("Operands does not fit %s", #TypeCheck);                     \
    }                                                                          \
    const auto instruction = std::make_shared<Type>(name, x, y, z);            \
    if (block != nullptr) [[likely]] {                                         \
        instruction->set_block(block);                                         \
    }                                                                          \
    instruction->add_operand(x);                                               \
    instruction->add_operand(y);                                               \
    instruction->add_operand(z);                                               \
    return instruction;                                                        \
}

CREATE_BINARY(Add, is_int32)
CREATE_BINARY(Sub, is_int32)
CREATE_BINARY(Mul, is_int32)
CREATE_BINARY(Div, is_int32)
CREATE_BINARY(Mod, is_int32)
CREATE_BINARY(And, is_int32)
CREATE_BINARY(Or, is_int32)
CREATE_BINARY(Xor, is_int32)
CREATE_BINARY(Smax, is_int32)
CREATE_BINARY(Smin, is_int32)

CREATE_BINARY(FAdd, is_float)
CREATE_BINARY(FSub, is_float)
CREATE_BINARY(FMul, is_float)
CREATE_BINARY(FDiv, is_float)
CREATE_BINARY(FMod, is_float)
CREATE_BINARY(FSmax, is_float)
CREATE_BINARY(FSmin, is_float)

CREATE_TERNARY(FMadd, is_float)
CREATE_TERNARY(FMsub, is_float)
CREATE_TERNARY(FNmadd, is_float)
CREATE_TERNARY(FNmsub, is_float)

std::shared_ptr<FNeg> FNeg::create(const std::string &name, const std::shared_ptr<Value> &value,
                                   const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<FNeg>(name, value);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

#undef CREATE_BINARY
#undef CREATE_TERNARY

std::shared_ptr<Phi> Phi::create(const std::string &name, const std::shared_ptr<Type::Type> &type,
                                 const std::shared_ptr<Block> &block,
                                 const Optional_Values &optional_values) {
    const auto instruction = std::make_shared<Phi>(name, type, optional_values);
    for (const auto &[block, value]: optional_values) {
        // block 和 value 均视为 phi 指令的操作数
        instruction->add_operand(block);
        instruction->add_operand(value);
    }
    // block 为nullptr时，不进行自动插入
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    return instruction;
}

// 为 phi 指令设置 optional_values
// 1. block不是键
// 2. block是键，且value是nullptr
void Phi::set_optional_value(const std::shared_ptr<Block> &block, const std::shared_ptr<Value> &optional_value) {
    if (*optional_value->get_type() != *type_) {
        log_error("Phi operand type must be same");
    }
    if (optional_values.find(block) == optional_values.end()) [[likely]] {
        block->add_user(shared_from_this()->as<User>());
    } else if (optional_values.at(block) != nullptr) {
        log_error("Should be nullptr: When setting optional value, there should be a new block");
    }
    optional_values[block] = optional_value;
    add_operand(optional_value);
}

void Phi::modify_operand(const std::shared_ptr<Value> &old_value, const std::shared_ptr<Value> &new_value) {
    User::modify_operand(old_value, new_value);
    if (const auto old_block = std::dynamic_pointer_cast<Block>(old_value)) {
        const auto new_block = std::dynamic_pointer_cast<Block>(new_value);
        if (new_block == nullptr) {
            log_error("Phi operand must be Block");
        }
        const auto it = optional_values.find(old_block);
        if (it == optional_values.end()) {
            log_error("Phi operand not found");
        }
        const auto value = it->second;
        optional_values.erase(it);
        optional_values[new_block] = value;
    } else {
        for (auto &[block, value]: optional_values) {
            if (value == old_value) [[likely]] {
                value = new_value;
            }
        }
    }
}

void Phi::remove_optional_value(const std::shared_ptr<Block> &block) {
    const auto value = optional_values.at(block);
    optional_values.erase(block);
    remove_operand(block);
    remove_operand(value);
}

std::shared_ptr<Block> Phi::find_optional_block(const std::shared_ptr<Value> &value) {
    const auto it = std::find_if(optional_values.begin(), optional_values.end(),
                                 [&](const auto &pair) { return pair.second == value; });
    return it != optional_values.end() ? it->first : nullptr;
}

std::shared_ptr<Select> Select::create(const std::string &name, const std::shared_ptr<Value> &condition,
                                       const std::shared_ptr<Value> &true_value,
                                       const std::shared_ptr<Value> &false_value,
                                       const std::shared_ptr<Block> &block) {
    if (*true_value->get_type() != *false_value->get_type()) {
        log_error("lhs and rhs should be same type");
    }
    if (condition->get_type() != Type::Integer::i1) {
        log_error("condition should be an i1");
    }
    const auto instruction = std::make_shared<Select>(name, condition, true_value, false_value);
    if (block != nullptr) [[likely]] { instruction->set_block(block); }
    instruction->add_operand(condition);
    instruction->add_operand(true_value);
    instruction->add_operand(false_value);
    return instruction;
}
}

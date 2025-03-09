#include "Mir/Instruction.h"

namespace Mir {
std::shared_ptr<Alloc> Alloc::create(const std::string &name, const std::shared_ptr<Type::Type> &type,
                                     const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Alloc>(name, type);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    return instruction;
}

std::shared_ptr<Load> Load::create(const std::string &name, const std::shared_ptr<Value> &addr,
                                   const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Load>(name, addr);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(addr);
    return instruction;
}

std::shared_ptr<Store> Store::create(const std::shared_ptr<Value> &addr,
                                     const std::shared_ptr<Value> &value,
                                     const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Store>(addr, value);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
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
std::shared_ptr<Type::Type> GetElementPtr::calc_type_(const std::shared_ptr<Value> &addr) {
    const auto type = addr->get_type();
    const auto ptr_type = std::dynamic_pointer_cast<Type::Pointer>(type);
    if (!ptr_type) {
        log_error("First operand must be a pointer type");
    }
    auto target_type = ptr_type->get_contain_type();
    if (const auto array_type = std::dynamic_pointer_cast<Type::Array>(target_type)) {
        return std::make_shared<Type::Pointer>(array_type->get_element_type());
    }
    if (target_type->is_int32() || target_type->is_float()) {
        return std::make_shared<Type::Pointer>(target_type);
    }
    log_fatal("Invalid pointer target type");
}

std::shared_ptr<GetElementPtr> GetElementPtr::create(const std::string &name,
                                                     const std::shared_ptr<Value> &addr,
                                                     const std::shared_ptr<Value> &index,
                                                     const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<GetElementPtr>(name, addr, index);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    const auto type = addr->get_type();
    const auto ptr_type = std::dynamic_pointer_cast<Type::Pointer>(type);
    if (!ptr_type) {
        log_error("First operand must be a pointer type");
    }
    instruction->add_operand(addr);
    if (const auto target_type = ptr_type->get_contain_type(); target_type->is_array()) {
        instruction->add_operand(std::make_shared<ConstInt>(0));
    }
    instruction->add_operand(index);
    return instruction;
}

std::shared_ptr<BitCast> BitCast::create(const std::string &name, const std::shared_ptr<Value> &value,
                                         const std::shared_ptr<Type::Type> &target_type,
                                         const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<BitCast>(name, value, target_type);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

std::shared_ptr<Fptosi> Fptosi::create(const std::string &name, const std::shared_ptr<Value> &value,
                                       const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Fptosi>(name, value);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

std::shared_ptr<Sitofp> Sitofp::create(const std::string &name, const std::shared_ptr<Value> &value,
                                       const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Sitofp>(name, value);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
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
            case Op::EQ: return std::make_shared<ConstBool>(*left == *right);
            case Op::NE: return std::make_shared<ConstBool>(*left != *right);
            case Op::GT: return std::make_shared<ConstBool>(*left > *right);
            case Op::LT: return std::make_shared<ConstBool>(*left < *right);
            case Op::GE: return std::make_shared<ConstBool>(*left >= *right);
            case Op::LE: return std::make_shared<ConstBool>(*left <= *right);
            default: break;
        }
    }
    if (lhs->is_constant() && !rhs->is_constant()) {
        std::swap(lhs, rhs);
        op = swap_op(op);
    }
    const auto instruction = std::make_shared<Fcmp>(name, op, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
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
            case Op::EQ: return std::make_shared<ConstBool>(*left == *right);
            case Op::NE: return std::make_shared<ConstBool>(*left != *right);
            case Op::GT: return std::make_shared<ConstBool>(*left > *right);
            case Op::LT: return std::make_shared<ConstBool>(*left < *right);
            case Op::GE: return std::make_shared<ConstBool>(*left >= *right);
            case Op::LE: return std::make_shared<ConstBool>(*left <= *right);
            default: break;
        }
    }
    if (lhs->is_constant() && !rhs->is_constant()) {
        std::swap(lhs, rhs);
        op = swap_op(op);
    }
    const auto instruction = std::make_shared<Icmp>(name, op, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<Zext> Zext::create(const std::string &name, const std::shared_ptr<Value> &value,
                                   const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Zext>(name, value);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

std::shared_ptr<Jump> Jump::create(const std::shared_ptr<Block> &target_block,
                                   const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Jump>(target_block);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(target_block);
    return instruction;
}

std::shared_ptr<Value> Branch::create(const std::shared_ptr<Value> &cond, const std::shared_ptr<Block> &true_block,
                                      const std::shared_ptr<Block> &false_block,
                                      const std::shared_ptr<Block> &block) {
    if (!cond->get_type()->is_int1()) { log_error("Cond must be an integer 1"); }
    if (cond->is_constant()) {
        if (const auto value = std::any_cast<int>(
            std::dynamic_pointer_cast<ConstBool>(cond)->get_constant_value()); value == 1) {
            return Jump::create(true_block, block);
        }
        return Jump::create(false_block, block);
    }
    const auto instruction = std::make_shared<Branch>(cond, true_block, false_block);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(cond);
    instruction->add_operand(true_block);
    instruction->add_operand(false_block);
    return instruction;
}

std::shared_ptr<Ret> Ret::create(const std::shared_ptr<Value> &value, const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Ret>(value);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(value);
    return instruction;
}

std::shared_ptr<Ret> Ret::create(const std::shared_ptr<Block> &block) {
    const auto instruction = std::make_shared<Ret>();
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
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
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(function);
    for (const auto &param: params) {
        instruction->add_operand(param);
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
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(function);
    for (const auto &param: params) {
        instruction->add_operand(param);
    }
    return instruction;
}

std::shared_ptr<Add> Add::create(const std::string &name, std::shared_ptr<Value> lhs,
                                 std::shared_ptr<Value> rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
        log_error("Operands must be int 32");
    }
    if (lhs->is_constant() && !rhs->is_constant()) {
        std::swap(lhs, rhs);
    }
    const auto instruction = std::make_shared<Add>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<Sub> Sub::create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                 const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
        log_error("Operands must be int 32");
    }
    const auto instruction = std::make_shared<Sub>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<Mul> Mul::create(const std::string &name, std::shared_ptr<Value> lhs,
                                 std::shared_ptr<Value> rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
        log_error("Operands must be int 32");
    }
    if (lhs->is_constant() && !rhs->is_constant()) {
        std::swap(lhs, rhs);
    }
    const auto instruction = std::make_shared<Mul>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<Div> Div::create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                 const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
        log_error("Operands must be int 32");
    }
    const auto instruction = std::make_shared<Div>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<Mod> Mod::create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                 const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
        log_error("Operands must be int 32");
    }
    const auto instruction = std::make_shared<Mod>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<FAdd> FAdd::create(const std::string &name, std::shared_ptr<Value> lhs,
                                   std::shared_ptr<Value> rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
        log_error("Operands must be float");
    }
    if (lhs->is_constant() && !rhs->is_constant()) {
        std::swap(lhs, rhs);
    }
    const auto instruction = std::make_shared<FAdd>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<FSub> FSub::create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                   const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
        log_error("Operands must be float");
    }
    const auto instruction = std::make_shared<FSub>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<FMul> FMul::create(const std::string &name, std::shared_ptr<Value> lhs,
                                   std::shared_ptr<Value> rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
        log_error("Operands must be float");
    }
    if (lhs->is_constant() && !rhs->is_constant()) {
        std::swap(lhs, rhs);
    }
    const auto instruction = std::make_shared<FMul>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<FDiv> FDiv::create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                   const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
        log_error("Operands must be float");
    }
    const auto instruction = std::make_shared<FDiv>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<FMod> FMod::create(const std::string &name, const std::shared_ptr<Value> &lhs,
                                   const std::shared_ptr<Value> &rhs, const std::shared_ptr<Block> &block) {
    if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
        log_error("Operands must be float");
    }
    const auto instruction = std::make_shared<FMod>(name, lhs, rhs);
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    instruction->add_operand(lhs);
    instruction->add_operand(rhs);
    return instruction;
}

std::shared_ptr<Phi> Phi::create(const std::string &name, const std::shared_ptr<Type::Type> &type,
                                 const std::shared_ptr<Block> &block,
                                 const Optional_Values &optional_values) {
    const auto instruction = std::make_shared<Phi>(name, type, optional_values);
    for (const auto &[block, value]: optional_values) {
        // block不是phi指令的操作数，在此只维护使用关系
        block->add_user(std::static_pointer_cast<User>(instruction));
    }
    // block 为nullptr时，不进行自动插入
    if (block != nullptr) [[unlikely]] { instruction->set_block(block); }
    return instruction;
}

void Phi::set_optional_value(const std::shared_ptr<Block> &block, const std::shared_ptr<Value> &optional_value) {
    if (*optional_value->get_type() != *type_) { log_error("Phi operand type must be same"); }
    optional_values[block] = optional_value;
    add_operand(optional_value);
}

void Phi::modify_operand(const std::shared_ptr<Value> &old_value, const std::shared_ptr<Value> &new_value) {
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
        old_block->delete_user(std::static_pointer_cast<User>(shared_from_this()));
        optional_values.erase(it);
        optional_values[new_block] = value;
        new_block->add_user(std::static_pointer_cast<User>(shared_from_this()));
    } else {
        User::modify_operand(old_value, new_value);
        for (auto &[block, value]: optional_values) {
            if (value == old_value) [[likely]] {
                value = new_value;
            }
        }
    }
}

void Phi::delete_optional_value(const std::shared_ptr<Block> &block) {
    const auto value = optional_values[block];
    optional_values.erase(block);
    remove_operand(block);
    remove_operand(value);
}
}

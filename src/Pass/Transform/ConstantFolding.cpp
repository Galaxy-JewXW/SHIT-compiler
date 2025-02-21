#include "Pass/Transform.h"

using namespace Mir;

template<typename T>
struct always_false : std::false_type {};

// 声明模板
template<typename Binary>
struct binary_traits;

// 对IntBinary指令进行特化
template<>
struct binary_traits<IntBinary> {
    using value_type = int;
    using constant_type = ConstInt;
};

// 对FloatBinary指令进行特化
template<>
struct binary_traits<FloatBinary> {
    using value_type = double;
    using constant_type = ConstFloat;
};

template<typename Binary>
static bool evaluate_binary(const std::shared_ptr<Binary> &inst, typename binary_traits<Binary>::value_type &res) {
    // 自动推导操作数的值类型和常量类型
    using T = typename binary_traits<Binary>::value_type;
    using ConstType = typename binary_traits<Binary>::constant_type;
    // 如果任一操作数不是常量，则无法进行常量折叠，返回false
    const auto lhs = inst->get_lhs(), rhs = inst->get_rhs();
    if (!lhs->is_constant() || !rhs->is_constant()) return false;
    if constexpr (std::is_same_v<T, int>) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    } else if constexpr (std::is_same_v<T, double>) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    }
    // 从常量指令中提取数值，根据操作符进行计算
    const T lhs_val = std::any_cast<T>(std::static_pointer_cast<ConstType>(lhs)->get_constant_value()),
            rhs_val = std::any_cast<T>(std::static_pointer_cast<ConstType>(rhs)->get_constant_value());
    switch (inst->op) {
        case Binary::Op::ADD: res = lhs_val + rhs_val;
            break;
        case Binary::Op::SUB: res = lhs_val - rhs_val;
            break;
        case Binary::Op::MUL: res = lhs_val * rhs_val;
            break;
        case Binary::Op::DIV: res = lhs_val / rhs_val;
            break;
        case Binary::Op::MOD: {
            if constexpr (std::is_same_v<T, double>) {
                res = std::fmod(lhs_val, rhs_val);
            } else {
                res = lhs_val % rhs_val;
            }
            break;
        }
        default: log_error("Unsupported operator in %s", inst->to_string().c_str());
    }
    return true;
}

template<typename Cmp>
struct cmp_traits {
    static_assert(always_false<Cmp>::value, "Unsupported Cmp type for evaluate_cmp");
};

// 对Icmp进行特化
template<>
struct cmp_traits<Icmp> {
    using value_type = int;
    using constant_type = ConstInt;
};

// 对Fcmp进行特化
template<>
struct cmp_traits<Fcmp> {
    using value_type = double;
    using constant_type = ConstFloat;
};

// 修改后的evaluate_cmp函数，只需传入指令类型Cmp，其余类型自动推导
template<typename Cmp>
static bool evaluate_cmp(const std::shared_ptr<Cmp> &inst, int &res) {
    // 自动推导比较操作的数值类型和对应的常量类型
    using T = typename cmp_traits<Cmp>::value_type;
    using ConstType = typename cmp_traits<Cmp>::constant_type;
    // 如果任一操作数不是常量，则无法进行折叠，返回false
    const auto lhs = inst->get_lhs(), rhs = inst->get_rhs();
    if (!lhs->is_constant() || !rhs->is_constant()) return false;
    // 检查左右操作数的类型是否合法
    if constexpr (std::is_same_v<T, int>) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    } else if constexpr (std::is_same_v<T, double>) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    }
    // 解引用静态指针获得常量值，根据比较操作符进行计算
    const auto lhs_val = *std::static_pointer_cast<ConstType>(lhs),
               rhs_val = *std::static_pointer_cast<ConstType>(rhs);
    switch (inst->op) {
        case Cmp::Op::EQ: res = lhs_val == rhs_val;
            break;
        case Cmp::Op::NE: res = lhs_val != rhs_val;
            break;
        case Cmp::Op::GT: res = lhs_val > rhs_val;
            break;
        case Cmp::Op::GE: res = lhs_val >= rhs_val;
            break;
        case Cmp::Op::LT: res = lhs_val < rhs_val;
            break;
        case Cmp::Op::LE: res = lhs_val <= rhs_val;
            break;
        default: log_error("Unsupported operator in %s", inst->to_string().c_str());
    }
    return true;
}

namespace Pass {
static bool _try_fold(const std::shared_ptr<Instruction> &instruction) {
    if (const Operator op = instruction->get_op(); op == Operator::INTBINARY) {
        const auto int_binary = std::static_pointer_cast<IntBinary>(instruction);
        if (int res_val; evaluate_binary(int_binary, res_val)) {
            const auto const_int = std::make_shared<ConstInt>(res_val);
            int_binary->replace_by_new_value(const_int);
            return true;
        }
    } else if (op == Operator::FLOATBINARY) {
        const auto float_binary = std::static_pointer_cast<FloatBinary>(instruction);
        if (double res_val; evaluate_binary(float_binary, res_val)) {
            const auto const_float = std::make_shared<ConstFloat>(res_val);
            float_binary->replace_by_new_value(const_float);
            return true;
        }
    } else if (op == Operator::ICMP) {
        const auto icmp = std::static_pointer_cast<Icmp>(instruction);
        if (int res_val; evaluate_cmp(icmp, res_val)) {
            const auto const_bool = std::make_shared<ConstBool>(res_val);
            icmp->replace_by_new_value(const_bool);
            return true;
        }
    } else if (op == Operator::FCMP) {
        const auto fcmp = std::static_pointer_cast<Fcmp>(instruction);
        if (int res_val; evaluate_cmp(fcmp, res_val)) {
            const auto const_bool = std::make_shared<ConstBool>(res_val);
            fcmp->replace_by_new_value(const_bool);
            return true;
        }
    }
    return false;
}

static bool fold(const std::shared_ptr<Function> &func) {
    bool changed = false;
    for (const auto &block: func->get_blocks()) {
        for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
            if (_try_fold(*it)) {
                (*it)->clear_operands();
                it = block->get_instructions().erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
    }
    return changed;
}

void ConstantFolding::transform(const std::shared_ptr<Module> module) {
    bool changed = false;
    do {
        for (const auto &func: *module) {
            changed |= fold(func);
        }
    } while (changed);
}
}

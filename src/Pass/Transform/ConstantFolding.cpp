#include "Pass/Transform.h"

using namespace Mir;

template<typename T>
struct always_false : std::false_type {};

template<typename Cmp>
struct cmp_traits {
    static_assert(always_false<Cmp>::value, "Unsupported Cmp type for evaluate_cmp");
};

template<>
struct cmp_traits<Icmp> {
    using type = int;
};

template<>
struct cmp_traits<Fcmp> {
    using type = double;
};

template<typename T, typename ConstType, typename Binary>
bool evaluate_binary(const std::shared_ptr<Binary> &inst, T &res) {
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

template<typename ConstType, typename Cmp>
bool evaluate_cmp(const std::shared_ptr<Cmp> &inst, int &res) {
    using T = typename cmp_traits<Cmp>::type;
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
        default:
            log_error("Unsupported operator in %s", inst->to_string().c_str());
    }
    return true;
}

namespace Pass {
bool _try_fold(const std::shared_ptr<Instruction> &instruction) {
    if (const Operator op = instruction->get_op(); op == Operator::INTBINARY) {
        const auto int_binary = std::static_pointer_cast<IntBinary>(instruction);
        if (int res_val; evaluate_binary<int, ConstInt, IntBinary>(int_binary, res_val)) {
            const auto const_int = std::make_shared<ConstInt>(res_val);
            int_binary->replace_by_new_value(const_int);
            return true;
        }
    } else if (op == Operator::FLOATBINARY) {
        const auto float_binary = std::static_pointer_cast<FloatBinary>(instruction);
        if (double res_val; evaluate_binary<double, ConstFloat, FloatBinary>(float_binary, res_val)) {
            const auto const_float = std::make_shared<ConstFloat>(res_val);
            float_binary->replace_by_new_value(const_float);
            return true;
        }
    } else if (op == Operator::ICMP) {
        const auto icmp = std::static_pointer_cast<Icmp>(instruction);
        if (int res_val; evaluate_cmp<ConstInt, Icmp>(icmp, res_val)) {
            const auto const_bool = std::make_shared<ConstBool>(res_val);
            icmp->replace_by_new_value(const_bool);
            return true;
        }
    } else if (op == Operator::FCMP) {
        const auto fcmp = std::static_pointer_cast<Fcmp>(instruction);
        if (int res_val; evaluate_cmp<ConstFloat, Fcmp>(fcmp, res_val)) {
            const auto const_bool = std::make_shared<ConstBool>(res_val);
            fcmp->replace_by_new_value(const_bool);
            return true;
        }
    }
    return false;
}

void run_on_function(const std::shared_ptr<Function> &func) {
    for (const auto &block: func->get_blocks()) {
        for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
            if (_try_fold(*it)) {
                (*it)->clear_operands();
                it = block->get_instructions().erase(it);
            } else {
                ++it;
            }
        }
    }
}

void ConstantFolding::transform(const std::shared_ptr<Module> module) {
    for (const auto &func: *module) {
        run_on_function(func);
    }
}
}

#include <optional>
#include <type_traits>

#include "Mir/Builder.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DCE.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Util.h"

using namespace Mir;

namespace {
// 替换指令
// 创建to时block应被设置为nullptr
void replace_instruction(const std::shared_ptr<Instruction> &from, const std::shared_ptr<Value> &to,
                         const std::shared_ptr<Block> &current_block,
                         std::vector<std::shared_ptr<Instruction>> &instructions, const size_t &idx) {
    from->replace_by_new_value(to);
    from->clear_operands();
    if (const auto target_inst{to->is<Instruction>()}) {
        if (target_inst->get_block() != nullptr) {
            return;
        }
        target_inst->set_block(current_block, false);
        instructions[idx] = target_inst;
    }
}

// 在idx的位置插入指令
void insert_instruction(const std::shared_ptr<Instruction> &instruction, const std::shared_ptr<Block> &current_block,
                        std::vector<std::shared_ptr<Instruction>> &instructions, size_t &idx) {
    instruction->set_block(current_block, false);
    instructions.insert(instructions.begin() + static_cast<long>(idx), instruction);
    ++idx;
}
} // namespace

namespace {
template<typename>
struct always_false : std::false_type {};

template<typename Compare>
struct Trait {
    static_assert(always_false<Compare>::value, "Trait not implemented for this type");
};

template<>
struct Trait<Icmp> {
    using Binary = IntBinary;
    using AddInst = Add;
    using SubInst = Sub;
    using MulInst = Mul;
    using DivInst = Div;
    using ConstantType = ConstInt;
    using Base = int;
};

template<>
struct Trait<Fcmp> {
    using Binary = FloatBinary;
    using AddInst = FAdd;
    using SubInst = FSub;
    using MulInst = FMul;
    using DivInst = FDiv;
    using ConstantType = ConstFloat;
    using Base = double;
};

template<typename Compare>
bool _reduce_cmp_with_mul(const std::shared_ptr<Compare> &cmp, std::vector<std::shared_ptr<Instruction>> &instructions,
                          const size_t &idx, const std::shared_ptr<Block> &current_block) {
    using ConstantType = typename Trait<Compare>::ConstantType;
    using Base = typename Trait<Compare>::Base;
    constexpr auto abs_ = 1e-7;
    constexpr auto zero{static_cast<Base>(false)}, one{static_cast<Base>(true)};

    if constexpr (!std::is_same_v<Compare, Icmp>) {
        return false;
    }

    const auto &lhs{cmp->get_lhs()}, &rhs{cmp->get_rhs()};
    const auto mul{lhs->template as<typename Trait<Compare>::MulInst>()};

    Base m{zero}, n{**rhs->template as<ConstantType>()};
    std::shared_ptr<Value> x{nullptr};
    if (mul->get_lhs()->is_constant()) {
        m = **mul->get_lhs()->template as<ConstantType>();
        x = mul->get_rhs();
    } else if (mul->get_rhs()->is_constant()) {
        m = **mul->get_rhs()->template as<ConstantType>();
        x = mul->get_lhs();
    }

    auto cmp_type{cmp->op};
    // m < 0
    if (std::abs(m) >= abs_ && m < zero) {
        m *= -one;
        n *= -one;
        cmp_type = Compare::swap_op(cmp_type);
    }

    switch (cmp_type) {
        case Compare::Op::EQ: {
            if (std::abs(m) < abs_) {
                cmp->replace_by_new_value(ConstBool::create(static_cast<int>(std::abs(n) < abs_)));
                return true;
            }
            if (const auto ans{Pass::Utils::safe_cal(n, m, std::modulus<>{})}) {
                if (const auto res{ans.value()}; std::abs(res) < abs_) {
                    const auto new_cmp = Compare::create("cmp", cmp_type, x, ConstantType::create(res), nullptr);
                    replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                    return true;
                }
                cmp->replace_by_new_value(ConstBool::create(0));
                return true;
            }
            break;
        }
        case Compare::Op::NE: {
            if (std::abs(m) < abs_) {
                cmp->replace_by_new_value(ConstBool::create(static_cast<int>(std::abs(n) >= abs_)));
                return true;
            }
            if (const auto ans{Pass::Utils::safe_cal(n, m, std::modulus<>{})}) {
                if (const auto res{ans.value()}; std::abs(res) < abs_) {
                    const auto new_cmp = Compare::create("cmp", cmp_type, x, ConstantType::create(res), nullptr);
                    replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                    return true;
                }
                cmp->replace_by_new_value(ConstBool::create(1));
                return true;
            }
            break;
        }
        case Compare::Op::LT: {
            // m * x < n -> x <= (n - 1) / m
            if (const auto ans{Pass::Utils::safe_cal(n - one, m, std::divides<>{})}) {
                const auto new_cmp =
                        Compare::create("cmp", Compare::Op::LE, x, ConstantType::create(ans.value()), nullptr);
                replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                return true;
            }
            break;
        }
        case Compare::Op::GT: {
            // m * x > n -> x >= (n + m) / m
            if (const auto ans{Pass::Utils::safe_cal(n + m, m, std::divides<>{})}) {
                const auto new_cmp =
                        Compare::create("cmp", Compare::Op::GE, x, ConstantType::create(ans.value()), nullptr);
                replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                return true;
            }
            break;
        }
        case Compare::Op::LE: {
            // m * x <= n -> x <= n / m
            if (const auto ans{Pass::Utils::safe_cal(n, m, std::divides<>{})}) {
                const auto new_cmp =
                        Compare::create("cmp", Compare::Op::LE, x, ConstantType::create(ans.value()), nullptr);
                replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                return true;
            }
            break;
        }
        case Compare::Op::GE: {
            // m * x >= n -> x >= (m + n - 1) / m
            if (const auto ans{Pass::Utils::safe_cal(m + n - one, m, std::divides<>{})}) {
                const auto new_cmp =
                        Compare::create("cmp", Compare::Op::GE, x, ConstantType::create(ans.value()), nullptr);
                replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                return true;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

template<typename Compare>
bool _reduce_cmp_with_div(const std::shared_ptr<Compare> &cmp, std::vector<std::shared_ptr<Instruction>> &instructions,
                          const size_t &idx, const std::shared_ptr<Block> &current_block) {
    using ConstantType = typename Trait<Compare>::ConstantType;
    using Base = typename Trait<Compare>::Base;
    constexpr auto abs_ = 1e-7;
    constexpr auto zero{static_cast<Base>(false)}, one{static_cast<Base>(true)};

    if constexpr (!std::is_same_v<Compare, Icmp>) {
        return false;
    }

    const auto &lhs{cmp->get_lhs()}, &rhs{cmp->get_rhs()};
    const auto div{lhs->template as<typename Trait<Compare>::DivInst>()};
    Base m{zero}, n{**rhs->template as<ConstantType>()};
    std::shared_ptr<Value> x{nullptr};
    if (div->get_rhs()->is_constant()) {
        m = **div->get_rhs()->template as<ConstantType>();
        x = div->get_lhs();
    } else if (div->get_rhs()->is_constant()) {
        return false;
    }

    if (std::abs(m) < abs_) {
        return false;
    }

    switch (const bool great_than_zero{m > zero}; const auto &cmp_type{cmp->op}) {
        case Compare::Op::EQ:
        case Compare::Op::NE:
            return false;
        case Compare::Op::LT: {
            if (const auto ans{Pass::Utils::safe_cal(n, m, std::multiplies<>{})}) {
                const auto t{great_than_zero ? Compare::Op::LT : Compare::Op::GT};
                const auto new_cmp = Compare::create("cmp", t, x, ConstantType::create(ans.value()), nullptr);
                replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                return true;
            }
            break;
        }
        case Compare::Op::GT: {
            if (const auto ans{Pass::Utils::safe_cal(n + one, m, std::multiplies<>{})}) {
                const auto t{great_than_zero ? Compare::Op::GE : Compare::Op::LE};
                const auto new_cmp = Compare::create("cmp", t, x, ConstantType::create(ans.value()), nullptr);
                replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                return true;
            }
            break;
        }
        case Compare::Op::LE: {
            if (const auto ans{Pass::Utils::safe_cal(n + one, m, std::multiplies<>{})}) {
                const auto t{great_than_zero ? Compare::Op::LT : Compare::Op::GT};
                const auto new_cmp = Compare::create("cmp", t, x, ConstantType::create(ans.value()), nullptr);
                replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                return true;
            }
            break;
        }
        case Compare::Op::GE: {
            if (const auto ans{Pass::Utils::safe_cal(n, m, std::multiplies<>{})}) {
                const auto t{great_than_zero ? Compare::Op::GE : Compare::Op::LE};
                const auto new_cmp = Compare::create("cmp", t, x, ConstantType::create(ans.value()), nullptr);
                replace_instruction(cmp, new_cmp, current_block, instructions, idx);
                return true;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

// %1 = icmp eq i32 %0, 0
// %2 = zext i1 %1 to i32
// %3 = icmp ne i32 %2, 0
template<typename Compare = Icmp>
bool _reduce_icmp_with_zext(std::vector<std::shared_ptr<Instruction>> &instructions, const size_t &idx,
                            const std::shared_ptr<Block> &) {
    static_assert(std::is_same_v<Compare, Icmp>);
    const auto cmp{instructions[idx]->as<Icmp>()};
    const auto &lhs{cmp->get_lhs()};

    if (const auto &rhs{cmp->get_rhs()}; **rhs->as<ConstInt>() != 0)
        return false;
    if (cmp->icmp_op() != Icmp::Op::NE)
        return false;

    const auto zext = lhs->as<Zext>();
    if (const auto icmp = zext->get_value()->is<Icmp>()) {
        if (!(!icmp->get_lhs()->is_constant() && icmp->get_rhs()->is_constant()))
            return false;
        if (**icmp->get_rhs()->as<ConstInt>() != 0)
            return false;
        cmp->replace_by_new_value(icmp);
        return true;
    }
    if (const auto fcmp = zext->get_value()->is<Fcmp>()) {
        if (!(!fcmp->get_lhs()->is_constant() && fcmp->get_rhs()->is_constant()))
            return false;
        if (**fcmp->get_rhs()->as<ConstFloat>() != 0.0)
            return false;
        cmp->replace_by_new_value(fcmp);
        return true;
    }
    return false;
}

template<typename Compare = Fcmp>
bool _reduce_fcmp_with_zext(std::vector<std::shared_ptr<Instruction>> &instructions, const size_t &idx,
                            const std::shared_ptr<Block> &) {
    static_assert(std::is_same_v<Compare, Fcmp>);
    const auto cmp{instructions[idx]->as<Fcmp>()};
    const auto &lhs{cmp->get_lhs()};

    if (const auto &rhs{cmp->get_rhs()}; **rhs->as<ConstFloat>() != 0.0)
        return false;
    if (cmp->fcmp_op() != Fcmp::Op::NE)
        return false;

    const auto sitofp = lhs->is<Sitofp>();
    if (!sitofp)
        return false;

    const auto zext = sitofp->get_value()->is<Zext>();
    if (!zext)
        return false;

    if (const auto fcmp = zext->get_value()->is<Fcmp>()) {
        if (!(!fcmp->get_lhs()->is_constant() && fcmp->get_rhs()->is_constant()))
            return false;
        if (**fcmp->get_rhs()->as<ConstFloat>() != 0.0)
            return false;
        if (fcmp->fcmp_op() != Fcmp::Op::NE)
            return false;
        cmp->replace_by_new_value(fcmp);
        return true;
    }
    return false;
}

template<typename Compare>
bool reduce_cmp(std::vector<std::shared_ptr<Instruction>> &instructions, const size_t &idx,
                const std::shared_ptr<Block> &current_block) {
    const auto cmp{instructions[idx]->as<Compare>()};

    if (cmp->get_lhs() == cmp->get_rhs()) {
        int ans;
        switch (cmp->op) {
            case Compare::Op::EQ:
            case Compare::Op::LE:
            case Compare::Op::GE:
                ans = 1;
                break;
            case Compare::Op::NE:
            case Compare::Op::LT:
            case Compare::Op::GT:
                ans = 0;
                break;
            default:
                return false;
        }
        cmp->replace_by_new_value(ConstInt::create(ans));
        return true;
    }

    int cnt{0};
    cnt += static_cast<int>(cmp->get_lhs()->is_constant());
    cnt += static_cast<int>(cmp->get_rhs()->is_constant());
    if (cnt != 1) {
        return false;
    }

    // using ConstantType = typename Trait<Compare>::ConstantType;
    // using Base = typename Trait<Compare>::Base;

    // ReSharper disable once CppTooWideScopeInitStatement
    const auto &lhs{cmp->get_lhs()}, &rhs{cmp->get_rhs()};
    if (lhs->is_constant() || !rhs->is_constant()) {
        log_fatal("Should handle before");
    }
    const auto inst{lhs->template is<typename Trait<Compare>::Binary>()};
    // const Base constant_value{**rhs->template as<ConstantType>()};
    if (inst == nullptr) {
        if constexpr (std::is_same_v<Compare, Icmp>) {
            if (const auto zext = lhs->template is<Zext>()) {
                return _reduce_icmp_with_zext<Compare>(instructions, idx, current_block);
            }
            return false;
        } else {
            return false;
        }
    }
    // const auto t{inst->op};
    // if (t == Trait<Compare>::Binary::Op::ADD) {
    //     const auto add_inst{inst->template as<typename Trait<Compare>::AddInst>()};
    //     // (3 + a) > 6 -> a > 3
    //     if (const auto &x{add_inst->get_lhs()}, &y{add_inst->get_rhs()}; x->is_constant()) {
    //         const auto cx{**x->template as<ConstantType>()};
    //         if (const auto ans{Pass::Utils::safe_cal(constant_value, cx, std::minus<>{})}) {
    //             const auto new_cmp = Compare::create("cmp", cmp->op, y, ConstantType::create(ans.value()), nullptr);
    //             replace_instruction(cmp, new_cmp, current_block, instructions, idx);
    //             return true;
    //         }
    //     } else if (y->is_constant()) {
    //         const auto cy{**y->template as<ConstantType>()};
    //         if (const auto ans{Pass::Utils::safe_cal(constant_value, cy, std::minus<>{})}) {
    //             const auto new_cmp = Compare::create("cmp", cmp->op, x, ConstantType::create(ans.value()), nullptr);
    //             replace_instruction(cmp, new_cmp, current_block, instructions, idx);
    //             return true;
    //         }
    //     }
    // } else if (t == Trait<Compare>::Binary::Op::SUB) {
    //     const auto sub_inst{inst->template as<typename Trait<Compare>::SubInst>()};
    //     // (a - 3) > 6 -> a > 9
    //     // (3 - a) > 6 -> a < -3
    //     if (const auto &x{sub_inst->get_lhs()}, &y{sub_inst->get_rhs()}; x->is_constant()) {
    //         const auto cx{**x->template as<ConstantType>()};
    //         if (const auto ans{Pass::Utils::safe_cal(cx, constant_value, std::minus<>{})}) {
    //             const auto new_cmp = Compare::create("cmp", Compare::swap_op(cmp->op), y,
    //                                                  ConstantType::create(ans.value()), nullptr);
    //             replace_instruction(cmp, new_cmp, current_block, instructions, idx);
    //             return true;
    //         }
    //     } else if (y->is_constant()) {
    //         const auto cy{**y->template as<ConstantType>()};
    //         if (const auto ans{Pass::Utils::safe_cal(constant_value, cy, std::plus<>{})}) {
    //             const auto new_cmp = Compare::create("cmp", cmp->op, x, ConstantType::create(ans.value()), nullptr);
    //             replace_instruction(cmp, new_cmp, current_block, instructions, idx);
    //             return true;
    //         }
    //     }
    // } else if (t == Trait<Compare>::Binary::Op::MUL) {
    //     return _reduce_cmp_with_mul<Compare>(cmp, instructions, idx, current_block);
    // } else if (t == Trait<Compare>::Binary::Op::DIV) {
    //     return _reduce_cmp_with_div<Compare>(cmp, instructions, idx, current_block);
    // }
    // // TODO: mod处理：x % 2 == 1 -> x & 1 == 1
    return false;
}
} // namespace

namespace {
[[nodiscard]]
bool reduce_add(const std::shared_ptr<Add> &add, std::vector<std::shared_ptr<Instruction>> &instructions, size_t &idx) {
    const auto current_block = add->get_block();
    const auto lhs = add->get_lhs(), rhs = add->get_rhs();
    // a + a = 2 * a
    if (lhs == rhs) {
        const auto new_mul = Mul::create(Builder::gen_variable_name(), lhs, ConstInt::create(2), nullptr);
        replace_instruction(add, new_mul, current_block, instructions, idx);
        return true;
    }
    if (rhs->is_constant()) {
        // a + 0 = 0
        const auto constant_rhs = std::static_pointer_cast<ConstInt>(rhs);
        if (constant_rhs->is_zero()) {
            add->replace_by_new_value(lhs);
            return true;
        }
        // (a + c1) + c2 = a + (c1 + c2)
        if (const auto add_lhs = std::dynamic_pointer_cast<Add>(lhs);
            add_lhs != nullptr && add_lhs->get_rhs()->is_constant()) {
            const auto c1 = std::static_pointer_cast<ConstInt>(add_lhs->get_rhs());
            const auto c = ConstInt::create(*c1 + *constant_rhs);
            const auto new_add = Add::create(Builder::gen_variable_name(), add_lhs->get_lhs(), c, nullptr);
            replace_instruction(add, new_add, current_block, instructions, idx);
            return true;
        }
        if (const auto sub_lhs = std::dynamic_pointer_cast<Sub>(lhs)) {
            // (a - c1) + c2 = a + (c2 - c1)
            const auto lhs1 = sub_lhs->get_lhs(), rhs1 = sub_lhs->get_rhs();
            if (!lhs1->is_constant() && rhs1->is_constant()) {
                const auto c1 = std::static_pointer_cast<ConstInt>(rhs1);
                const auto c = ConstInt::create(*constant_rhs - *c1);
                const auto new_add = Add::create(Builder::gen_variable_name(), lhs1, c, nullptr);
                replace_instruction(add, new_add, current_block, instructions, idx);
                return true;
            }
            // (c1 - a) + c2 = (c1 + c2) - a
            if (lhs1->is_constant() && !rhs1->is_constant()) {
                const auto c1 = std::static_pointer_cast<ConstInt>(lhs1);
                const auto c = ConstInt::create(*c1 + *constant_rhs);
                const auto new_sub = Sub::create(Builder::gen_variable_name(), c, rhs1, nullptr);
                replace_instruction(add, new_sub, current_block, instructions, idx);
                return true;
            }
        }
    }
    // a + (-b) = a - b
    if (const auto sub_rhs = std::dynamic_pointer_cast<Sub>(rhs)) {
        if (sub_rhs->get_lhs()->is_constant() && std::static_pointer_cast<ConstInt>(sub_rhs->get_lhs())->is_zero()) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), lhs, sub_rhs->get_rhs(), nullptr);
            replace_instruction(add, new_sub, current_block, instructions, idx);
            return true;
        }
    }
    // (-b) + a = a - b
    if (const auto sub_lhs = std::dynamic_pointer_cast<Sub>(lhs)) {
        if (sub_lhs->get_lhs()->is_constant() && std::static_pointer_cast<ConstInt>(sub_lhs->get_lhs())->is_zero()) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), rhs, sub_lhs->get_rhs(), nullptr);
            replace_instruction(add, new_sub, current_block, instructions, idx);
            return true;
        }
    }
    // b * a + c * a = (b + c) * a
    // a * b + c * a = (b + c) * a
    // a * b + a * c = (b + c) * a
    // b * a + a * c = (b + c) * a
    if (const auto mul_lhs = std::dynamic_pointer_cast<Mul>(lhs), mul_rhs = std::dynamic_pointer_cast<Mul>(rhs);
        mul_lhs != nullptr && mul_rhs != nullptr) {
        const auto x = mul_lhs->get_lhs(), y = mul_lhs->get_rhs(), z = mul_rhs->get_lhs(), w = mul_rhs->get_rhs();
        if (y == w) {
            const auto new_add = Add::create(Builder::gen_variable_name(), x, z, nullptr);
            insert_instruction(new_add, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_add, y, nullptr);
            replace_instruction(add, new_mul, current_block, instructions, idx);
            return true;
        }
        if (x == w) {
            const auto new_add = Add::create(Builder::gen_variable_name(), y, z, nullptr);
            insert_instruction(new_add, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_add, x, nullptr);
            replace_instruction(add, new_mul, current_block, instructions, idx);
            return true;
        }
        if (x == z) {
            const auto new_add = Add::create(Builder::gen_variable_name(), y, w, nullptr);
            insert_instruction(new_add, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_add, x, nullptr);
            replace_instruction(add, new_mul, current_block, instructions, idx);
            return true;
        }
        if (y == z) {
            const auto new_add = Add::create(Builder::gen_variable_name(), x, w, nullptr);
            insert_instruction(new_add, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_add, y, nullptr);
            replace_instruction(add, new_mul, current_block, instructions, idx);
            return true;
        }
    }
    // a * b + a = (1 + b) * a
    // a * b + b = (1 + a) * b
    if (const auto mul_lhs = std::dynamic_pointer_cast<Mul>(lhs)) {
        const auto a = mul_lhs->get_lhs(), b = mul_lhs->get_rhs();
        const auto const_one = ConstInt::create(1);
        if (a == rhs) {
            const auto new_add = Add::create(Builder::gen_variable_name(), b, const_one, nullptr);
            insert_instruction(new_add, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_add, a, nullptr);
            replace_instruction(add, new_mul, current_block, instructions, idx);
            return true;
        }
        if (b == rhs) {
            const auto new_add = Add::create(Builder::gen_variable_name(), a, const_one, nullptr);
            insert_instruction(new_add, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_add, b, nullptr);
            replace_instruction(add, new_mul, current_block, instructions, idx);
            return true;
        }
    }
    // a + a * b = (1 + b) * a
    // b + a * b = (1 + a) * b
    if (const auto mul_rhs = std::dynamic_pointer_cast<Mul>(rhs)) {
        const auto a = mul_rhs->get_lhs(), b = mul_rhs->get_rhs();
        const auto const_one = ConstInt::create(1);
        if (a == lhs) {
            const auto new_add = Add::create(Builder::gen_variable_name(), b, const_one, nullptr);
            insert_instruction(new_add, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_add, a, nullptr);
            replace_instruction(add, new_mul, current_block, instructions, idx);
            return true;
        }
        if (b == lhs) {
            const auto new_add = Add::create(Builder::gen_variable_name(), a, const_one, nullptr);
            insert_instruction(new_add, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_add, b, nullptr);
            replace_instruction(add, new_mul, current_block, instructions, idx);
            return true;
        }
    }
    return false;
}

[[nodiscard]]
bool reduce_sub(const std::shared_ptr<Sub> &sub, std::vector<std::shared_ptr<Instruction>> &instructions, size_t &idx) {
    const auto current_block = sub->get_block();
    const auto lhs = sub->get_lhs(), rhs = sub->get_rhs();
    // a - a = 0
    if (lhs == rhs) {
        const auto const_zero = ConstInt::create(0);
        replace_instruction(sub, const_zero, current_block, instructions, idx);
        return true;
    }
    // a - (-b) = a + b
    if (const auto sub_rhs = std::dynamic_pointer_cast<Sub>(rhs)) {
        if (sub_rhs->get_lhs()->is_constant() && std::static_pointer_cast<ConstInt>(sub_rhs->get_lhs())->is_zero()) {
            const auto new_add = Add::create(Builder::gen_variable_name(), lhs, sub_rhs->get_rhs(), nullptr);
            replace_instruction(sub, new_add, current_block, instructions, idx);
            return true;
        }
    }
    if (const auto add_lhs = std::dynamic_pointer_cast<Add>(lhs)) {
        // (a + b) - a = b
        // (b + a) - a = b
        const auto a = add_lhs->get_lhs(), b = add_lhs->get_rhs();
        if (a == rhs) {
            replace_instruction(sub, b, current_block, instructions, idx);
            return true;
        }
        if (b == rhs) {
            replace_instruction(sub, a, current_block, instructions, idx);
            return true;
        }
    }
    if (const auto add_rhs = std::dynamic_pointer_cast<Add>(rhs)) {
        // a - (a + b) = -b
        // a - (b + a) = -b
        const auto a = add_rhs->get_lhs(), b = add_rhs->get_rhs();
        if (lhs == a) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), ConstInt::create(0), b, nullptr);
            replace_instruction(sub, new_sub, current_block, instructions, idx);
            return true;
        }
        if (lhs == b) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), ConstInt::create(0), a, nullptr);
            replace_instruction(sub, new_sub, current_block, instructions, idx);
            return true;
        }
    }
    if (lhs->is_constant()) {
        const auto constant_lhs = std::static_pointer_cast<ConstInt>(lhs);
        if (constant_lhs->is_zero()) {
            if (const auto sub_rhs = std::dynamic_pointer_cast<Sub>(rhs)) {
                // 0 - (-a) = a
                if (sub_rhs->get_lhs()->is_constant() &&
                    std::static_pointer_cast<ConstInt>(sub_rhs->get_lhs())->is_zero()) {
                    replace_instruction(sub, sub_rhs->get_rhs(), current_block, instructions, idx);
                    return true;
                }
                // 0 - (a - b) = b - a;
                const auto a = sub_rhs->get_lhs(), b = sub_rhs->get_rhs();
                const auto new_sub = Sub::create(Builder::gen_variable_name(), b, a, nullptr);
                replace_instruction(sub, new_sub, current_block, instructions, idx);
                return true;
            }
        }
        // c1 - (x + c2) = (c1 - c2) - x
        if (const auto add_rhs = std::dynamic_pointer_cast<Add>(rhs)) {
            if (const auto c2 = std::dynamic_pointer_cast<ConstInt>(add_rhs->get_rhs())) {
                const auto c = ConstInt::create(*constant_lhs - *c2);
                const auto new_sub = Sub::create(Builder::gen_variable_name(), c, add_rhs->get_lhs(), nullptr);
                replace_instruction(sub, new_sub, current_block, instructions, idx);
                return true;
            }
        }
        // c1 - (x - c2) = (c1 + c2) - x
        if (const auto sub_rhs = std::dynamic_pointer_cast<Sub>(rhs)) {
            if (const auto c2 = std::dynamic_pointer_cast<ConstInt>(sub_rhs->get_rhs())) {
                const auto c = ConstInt::create(*constant_lhs + *c2);
                const auto new_sub = Sub::create(Builder::gen_variable_name(), c, sub_rhs->get_lhs(), nullptr);
                replace_instruction(sub, new_sub, current_block, instructions, idx);
                return true;
            }
        }
    }
    if (rhs->is_constant()) {
        const auto constant_rhs = std::static_pointer_cast<ConstInt>(rhs);
        // a - 0 = a
        if (constant_rhs->is_zero()) {
            sub->replace_by_new_value(lhs);
            return true;
        }
        // (a + c1) - c2 = a + (c1 - c2)
        if (const auto add_lhs = std::dynamic_pointer_cast<Add>(lhs)) {
            if (const auto c1 = std::dynamic_pointer_cast<ConstInt>(add_lhs->get_rhs())) {
                const auto c = ConstInt::create(*c1 - *constant_rhs);
                const auto new_add = Add::create(Builder::gen_variable_name(), add_lhs->get_lhs(), c, nullptr);
                replace_instruction(sub, new_add, current_block, instructions, idx);
                return true;
            }
        }
        if (const auto sub_lhs = std::dynamic_pointer_cast<Sub>(lhs)) {
            // (a - c1) - c2 = a - (c1 + c2)
            if (const auto c1 = std::dynamic_pointer_cast<ConstInt>(sub_lhs->get_rhs())) {
                const auto c = ConstInt::create(*c1 + *constant_rhs);
                const auto new_sub = Sub::create(Builder::gen_variable_name(), sub_lhs->get_lhs(), c, nullptr);
                replace_instruction(sub, new_sub, current_block, instructions, idx);
                return true;
            }
            // (c1 - a) - c2 = (c1 - c2) - a
            if (const auto c1 = std::dynamic_pointer_cast<ConstInt>(sub_lhs->get_lhs())) {
                const auto c = ConstInt::create(*c1 - *constant_rhs);
                const auto new_sub = Sub::create(Builder::gen_variable_name(), c, sub_lhs->get_rhs(), nullptr);
                replace_instruction(sub, new_sub, current_block, instructions, idx);
                return true;
            }
        }
    }
    // a * b - a = a * (b - 1)
    // b * a - a = a * (b - 1)
    if (const auto mul_lhs = std::dynamic_pointer_cast<Mul>(lhs)) {
        const auto a = mul_lhs->get_lhs(), b = mul_lhs->get_rhs();
        if (a == rhs) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), b, ConstInt::create(1), nullptr);
            insert_instruction(new_sub, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_sub, a, nullptr);
            replace_instruction(sub, new_mul, current_block, instructions, idx);
            return true;
        }
        if (b == rhs) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), a, ConstInt::create(1), nullptr);
            insert_instruction(new_sub, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_sub, b, nullptr);
            replace_instruction(sub, new_mul, current_block, instructions, idx);
            return true;
        }
    }
    // b * a - c * a = (b - c) * a
    // a * b - c * a = (b - c) * a
    // a * b - a * c = (b - c) * a
    // b * a - a * c = (b - c) * a
    if (const auto mul_lhs = std::dynamic_pointer_cast<Mul>(lhs), mul_rhs = std::dynamic_pointer_cast<Mul>(rhs);
        mul_lhs != nullptr && mul_rhs != nullptr) {
        const auto x = mul_lhs->get_lhs(), y = mul_lhs->get_rhs(), z = mul_rhs->get_lhs(), w = mul_rhs->get_rhs();
        if (y == w) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), x, z, nullptr);
            insert_instruction(new_sub, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_sub, y, nullptr);
            replace_instruction(sub, new_mul, current_block, instructions, idx);
            return true;
        }
        if (x == w) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), y, z, nullptr);
            insert_instruction(new_sub, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_sub, x, nullptr);
            replace_instruction(sub, new_mul, current_block, instructions, idx);
            return true;
        }
        if (x == z) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), y, w, nullptr);
            insert_instruction(new_sub, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_sub, x, nullptr);
            replace_instruction(sub, new_mul, current_block, instructions, idx);
            return true;
        }
        if (y == z) {
            const auto new_add = Sub::create(Builder::gen_variable_name(), x, w, nullptr);
            insert_instruction(new_add, current_block, instructions, idx);
            const auto new_mul = Mul::create(Builder::gen_variable_name(), new_add, y, nullptr);
            replace_instruction(sub, new_mul, current_block, instructions, idx);
            return true;
        }
    }
    return false;
}

[[nodiscard]]
bool reduce_mul(const std::shared_ptr<Mul> &mul, std::vector<std::shared_ptr<Instruction>> &instructions,
                const size_t &idx) {
    const auto current_block = mul->get_block();
    const auto lhs = mul->get_lhs();
    if (const auto rhs = mul->get_rhs(); rhs->is_constant()) [[unlikely]] {
        const auto constant_rhs = rhs->as<ConstInt>();
        const auto zero = ConstInt::create(0);
        // a * 0 = 0
        if (constant_rhs->is_zero()) {
            mul->replace_by_new_value(zero);
            return true;
        }
        const int constant_rhs_v = constant_rhs->get<int>();
        // a * 1 = a
        if (constant_rhs_v == 1) {
            mul->replace_by_new_value(lhs);
            return true;
        }
        // a * (-1) = 0 - a
        if (constant_rhs_v == -1) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), zero, lhs, nullptr);
            replace_instruction(mul, new_sub, current_block, instructions, idx);
            return true;
        }
        // (-a) * c = a * (-c)
        if (const auto sub_lhs = std::dynamic_pointer_cast<Sub>(lhs)) {
            if (const auto c1 = std::dynamic_pointer_cast<ConstInt>(sub_lhs->get_lhs());
                c1 != nullptr && c1->is_zero()) {
                const auto c = ConstInt::create(-constant_rhs_v);
                const auto new_mul = Mul::create(Builder::gen_variable_name(), sub_lhs->get_rhs(), c, nullptr);
                replace_instruction(mul, new_mul, current_block, instructions, idx);
                return true;
            }
        }
        // (a * c1) * c2 = a * (c1 * c2)
        if (const auto mul_lhs = std::dynamic_pointer_cast<Mul>(lhs)) {
            const auto &c2 = constant_rhs;
            if (const auto c1 = mul_lhs->get_rhs(); c1->is_constant()) {
                const auto c = ConstInt::create(*c1->as<ConstInt>() * *c2);
                const auto new_mul = Mul::create(Builder::gen_variable_name(), mul_lhs->get_lhs(), c, nullptr);
                replace_instruction(mul, new_mul, current_block, instructions, idx);
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]]
bool reduce_div(const std::shared_ptr<Div> &div, std::vector<std::shared_ptr<Instruction>> &instructions,
                const size_t &idx) {
    const auto current_block = div->get_block();
    const auto lhs = div->get_lhs(), rhs = div->get_rhs();
    // a / a = 1
    if (lhs == rhs) {
        div->replace_by_new_value(ConstInt::create(1));
        return true;
    }
    // a / (-a) = -1
    if (const auto sub_rhs = std::dynamic_pointer_cast<Sub>(rhs)) {
        if (const auto c1 = std::dynamic_pointer_cast<ConstInt>(sub_rhs->get_lhs());
            c1 != nullptr && c1->is_zero() && sub_rhs->get_rhs() == lhs) {
            div->replace_by_new_value(ConstInt::create(-1));
            return true;
        }
    }
    if (lhs->is_constant()) {
        // 0 / a = 0
        if (const auto constant_lhs = std::static_pointer_cast<ConstInt>(lhs); constant_lhs->is_zero()) {
            div->replace_by_new_value(ConstInt::create(0));
            return true;
        }
    }
    if (rhs->is_constant()) {
        const auto constant_rhs = std::static_pointer_cast<ConstInt>(rhs);
        const int constant_rhs_v = constant_rhs->get<int>();
        // a / 1 = a
        if (constant_rhs_v == 1) {
            div->replace_by_new_value(lhs);
            return true;
        }
        // a / (-1) = 0 - a
        if (constant_rhs_v == -1) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), ConstInt::create(0), lhs, nullptr);
            replace_instruction(div, new_sub, current_block, instructions, idx);
            return true;
        }
        if (const auto mul_lhs = std::dynamic_pointer_cast<Mul>(lhs)) {
            // (a * c2) / c1 = x * (c2 / c1), when c2 % c1 == 0
            if (const auto c2 = std::dynamic_pointer_cast<ConstInt>(mul_lhs->get_rhs())) {
                if (const int c2_v = c2->get<int>(); c2_v % constant_rhs_v == 0) {
                    const auto c = ConstInt::create(c2_v / constant_rhs_v);
                    const auto new_mul = Mul::create(Builder::gen_variable_name(), mul_lhs->get_lhs(), c, nullptr);
                    replace_instruction(div, new_mul, current_block, instructions, idx);
                    return true;
                }
            }
        }
        if (const auto sub_lhs = std::dynamic_pointer_cast<Sub>(lhs)) {
            // (-a) / c = a / (-c)
            if (const auto c1 = std::dynamic_pointer_cast<ConstInt>(sub_lhs->get_lhs());
                c1 != nullptr && c1->is_zero()) {
                const auto c = ConstInt::create(-constant_rhs_v);
                const auto new_div = Div::create(Builder::gen_variable_name(), sub_lhs->get_rhs(), c, nullptr);
                replace_instruction(div, new_div, current_block, instructions, idx);
                return true;
            }
        }
    }
    // (-a) / a = 1
    if (const auto sub_lhs = std::dynamic_pointer_cast<Sub>(lhs)) {
        if (const auto c1 = std::dynamic_pointer_cast<ConstInt>(sub_lhs->get_lhs()); c1 != nullptr && c1->is_zero()) {
            if (sub_lhs->get_rhs() == rhs) {
                div->replace_by_new_value(ConstInt::create(1));
                return true;
            }
        }
    }
    if (const auto mul_rhs = std::dynamic_pointer_cast<Mul>(rhs)) {
        const auto x = mul_rhs->get_lhs(), y = mul_rhs->get_rhs();
        // a / (a * b) = 1 / b
        if (lhs == x) {
            const auto new_div = Div::create(Builder::gen_variable_name(), ConstInt::create(1), y, nullptr);
            replace_instruction(div, new_div, current_block, instructions, idx);
            return true;
        }
        // a / (b * a) = 1 / b
        if (lhs == y) {
            const auto new_div = Div::create(Builder::gen_variable_name(), ConstInt::create(1), x, nullptr);
            replace_instruction(div, new_div, current_block, instructions, idx);
            return true;
        }
    }
    return false;
}

[[nodiscard]]
bool reduce_mod(const std::shared_ptr<Mod> &mod, std::vector<std::shared_ptr<Instruction>> &instructions,
                const size_t &idx) {
    const auto current_block = mod->get_block();
    const auto lhs = mod->get_lhs(), rhs = mod->get_rhs();
    // a % a = 0
    if (lhs == rhs) {
        mod->replace_by_new_value(ConstInt::create(1));
        return true;
    }
    if (lhs->is_constant()) {
        // 0 % a = 0
        if (const auto constant_lhs = std::static_pointer_cast<ConstInt>(lhs); constant_lhs->is_zero()) {
            mod->replace_by_new_value(ConstInt::create(0));
            return true;
        }
    }
    if (rhs->is_constant()) {
        const auto constant_rhs = std::static_pointer_cast<ConstInt>(rhs);
        const int constant_rhs_v = constant_rhs->get<int>();
        // a % 1 = 0
        if (constant_rhs_v == 1 || constant_rhs_v == -1) {
            mod->replace_by_new_value(ConstInt::create(0));
            return true;
        }
        if (const auto mul_lhs = std::dynamic_pointer_cast<Mul>(lhs)) {
            // (a * c2) % c1 = x * (c2 % c1), when c2 % c1 == 0
            if (const auto c2 = std::dynamic_pointer_cast<ConstInt>(mul_lhs->get_rhs())) {
                if (const int c2_v = c2->get<int>(); c2_v % constant_rhs_v == 0) {
                    const auto c = ConstInt::create(c2_v % constant_rhs_v);
                    const auto new_mul = Mul::create(Builder::gen_variable_name(), mul_lhs->get_lhs(), c, nullptr);
                    replace_instruction(mod, new_mul, current_block, instructions, idx);
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]]
bool reduce_max(const std::shared_ptr<Smax> &smax, std::vector<std::shared_ptr<Instruction>> &, const size_t &) {
    // max(a, a) = a
    if (smax->get_lhs() == smax->get_rhs()) {
        smax->replace_by_new_value(smax->get_lhs());
        return true;
    }
    // max(max(a, b), c) = max(a, b), if a == c or b == c
    if (const auto smax_lhs{smax->get_lhs()->is<Smax>()}) {
        if (smax_lhs->get_lhs() == smax->get_rhs() || smax_lhs->get_rhs() == smax->get_rhs()) {
            smax->replace_by_new_value(smax_lhs);
            return true;
        }
    }
    // max(a, max(b, c)) = max(b, c), if a == b or a == c
    if (const auto smax_rhs{smax->get_rhs()->is<Smax>()}) {
        if (smax_rhs->get_lhs() == smax->get_lhs() || smax_rhs->get_rhs() == smax->get_lhs()) {
            smax->replace_by_new_value(smax_rhs);
            return true;
        }
    }
    // max(min(a, b), c) = c, if a == c or b == c
    if (const auto smin_lhs{smax->get_lhs()->is<Smin>()}) {
        if (smin_lhs->get_lhs() == smax->get_rhs() || smin_lhs->get_rhs() == smax->get_rhs()) {
            smax->replace_by_new_value(smax->get_rhs());
            return true;
        }
    }
    // max(a, min(b, c)) = a, if a == b or a == c
    if (const auto smin_rhs{smax->get_rhs()->is<Smin>()}) {
        if (smin_rhs->get_lhs() == smax->get_lhs() || smin_rhs->get_rhs() == smax->get_lhs()) {
            smax->replace_by_new_value(smax->get_lhs());
            return true;
        }
    }
    return false;
}

[[nodiscard]]
bool reduce_min(const std::shared_ptr<Smin> &smin, std::vector<std::shared_ptr<Instruction>> &, const size_t &) {
    // min(a, a) = a
    if (smin->get_lhs() == smin->get_rhs()) {
        smin->replace_by_new_value(smin->get_lhs());
        return true;
    }
    // min(max(a, b), c) = c, if a == c or b == c
    if (const auto smax_lhs{smin->get_lhs()->is<Smax>()}) {
        if (smax_lhs->get_lhs() == smin->get_rhs() || smax_lhs->get_rhs() == smin->get_rhs()) {
            smin->replace_by_new_value(smin->get_rhs());
            return true;
        }
    }
    // min(a, max(b, c)) = a, if a == b or a == c
    if (const auto smax_rhs{smin->get_rhs()->is<Smax>()}) {
        if (smax_rhs->get_lhs() == smin->get_lhs() || smax_rhs->get_rhs() == smin->get_lhs()) {
            smin->replace_by_new_value(smin->get_lhs());
            return true;
        }
    }
    // min(min(a, b), c) = min(a, b), if a == c or b == c
    if (const auto smin_lhs{smin->get_lhs()->is<Smin>()}) {
        if (smin_lhs->get_lhs() == smin->get_rhs() || smin_lhs->get_rhs() == smin->get_rhs()) {
            smin->replace_by_new_value(smin_lhs);
            return true;
        }
    }
    // min(a, min(b, c)) = min(b, c), if a == b or a == c
    if (const auto smin_rhs{smin->get_rhs()->is<Smin>()}) {
        if (smin_rhs->get_lhs() == smin->get_lhs() || smin_rhs->get_rhs() == smin->get_lhs()) {
            smin->replace_by_new_value(smin_rhs);
            return true;
        }
    }
    return false;
}

[[nodiscard]]
bool handle_intbinary_icmp(const std::shared_ptr<Block> &block) {
    auto &instructions = block->get_instructions();
    bool changed = false;
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (const auto t{instructions[i]->get_op()}; t == Operator::INTBINARY) {
            changed |= [&]() -> bool {
                switch (const auto binary{instructions[i]->as<IntBinary>()}; binary->intbinary_op()) {
                    case IntBinary::Op::ADD:
                        return reduce_add(binary->as<Add>(), instructions, i);
                    case IntBinary::Op::SUB:
                        return reduce_sub(binary->as<Sub>(), instructions, i);
                    case IntBinary::Op::MUL:
                        return reduce_mul(binary->as<Mul>(), instructions, i);
                    case IntBinary::Op::DIV:
                        return reduce_div(binary->as<Div>(), instructions, i);
                    case IntBinary::Op::MOD:
                        return reduce_mod(binary->as<Mod>(), instructions, i);
                    case IntBinary::Op::SMAX:
                        return reduce_max(binary->as<Smax>(), instructions, i);
                    case IntBinary::Op::SMIN:
                        return reduce_min(binary->as<Smin>(), instructions, i);
                    default:
                        return false;
                }
            }();
        } else if (t == Operator::ICMP) {
            changed |= reduce_cmp<Icmp>(instructions, i, block);
        } else if (t == Operator::FCMP) {
            changed |= _reduce_fcmp_with_zext<Fcmp>(instructions, i, block);
        }
    }
    return changed;
}

[[maybe_unused]]
bool handle_float_ternary(const std::shared_ptr<Function> &func) {
    bool changed{false};
    static constexpr double abs_ = 1e-6f;

    const auto handle_fneg = [&](const std::shared_ptr<Block> &block) -> void {
        auto &instructions{block->get_instructions()};
        for (size_t i{0}; i < instructions.size(); ++i) {
            if (instructions[i]->get_op() != Operator::FLOATBINARY) [[likely]] {
                continue;
            }
            const auto fb{instructions[i]->as<FloatBinary>()};
            if (const auto t{fb->floatbinary_op()}; t == FloatBinary::Op::MUL) {
                const auto fmul{fb->as<FMul>()};
                const auto candidate = [&]() -> std::optional<std::shared_ptr<Value>> {
                    if (fmul->get_lhs()->is_constant() && std::fabs(**fmul->get_lhs()->as<ConstFloat>() + 1.0) < abs_) {
                        return fmul->get_rhs();
                    }
                    if (fmul->get_rhs()->is_constant() && std::fabs(**fmul->get_rhs()->as<ConstFloat>() + 1.0) < abs_) {
                        return fmul->get_lhs();
                    }
                    return std::nullopt;
                }();
                if (candidate.has_value()) {
                    const auto fneg{FNeg::create("fneg", candidate.value(), nullptr)};
                    replace_instruction(fb, fneg, block, instructions, i);
                    changed = true;
                }
            } else if (t == FloatBinary::Op::SUB) {
                if (const auto fsub{fb->as<FSub>()};
                    fsub->get_lhs()->is_constant() && std::fabs(**fsub->get_lhs()->as<ConstFloat>()) < abs_) {
                    const auto fneg{FNeg::create("fneg", fsub->get_rhs(), nullptr)};
                    replace_instruction(fb, fneg, block, instructions, i);
                    changed = true;
                }
            } else if (t == FloatBinary::Op::DIV) {
                if (const auto fdiv{fb->as<FDiv>()};
                    fdiv->get_rhs()->is_constant() && std::fabs(**fdiv->get_rhs()->as<ConstFloat>() + 1.0) < abs_) {
                    const auto fneg{FNeg::create("fneg", fdiv->get_lhs(), nullptr)};
                    replace_instruction(fb, fneg, block, instructions, i);
                    changed = true;
                }
            }
        }
    };

    const auto handle_fmadd_fmsub = [&](const std::shared_ptr<Block> &block) -> void {
        auto &instructions{block->get_instructions()};
        for (size_t i{0}; i < instructions.size(); ++i) {
            if (instructions[i]->get_op() != Operator::FLOATBINARY) [[likely]] {
                continue;
            }
            const auto fb{instructions[i]->as<FloatBinary>()};
            if (const auto t{fb->floatbinary_op()}; t == FloatBinary::Op::ADD) {
                const auto fadd{fb->as<FAdd>()};
                const auto new_inst = [&]() -> std::optional<std::shared_ptr<Instruction>> {
                    if (const auto fmul1{fadd->get_lhs()->is<FMul>()}) {
                        // (fa * fb) + fc = fmadd(fa, fb, fc)
                        return std::make_optional(
                                FMadd::create("fmadd", fmul1->get_lhs(), fmul1->get_rhs(), fadd->get_rhs(), nullptr));
                    }
                    if (const auto fmul2{fadd->get_rhs()->is<FMul>()}) {
                        // fa + (fb * fc) = fmadd(fb, fc, fa)
                        return std::make_optional(
                                FMadd::create("fmadd", fmul2->get_lhs(), fmul2->get_rhs(), fadd->get_lhs(), nullptr));
                    }
                    return std::nullopt;
                }();
                if (new_inst.has_value()) {
                    replace_instruction(fb, new_inst.value(), block, instructions, i);
                    changed = true;
                }
            } else if (t == FloatBinary::Op::SUB) {
                const auto fsub{fb->as<FSub>()};
                const auto new_inst = [&]() -> std::optional<std::shared_ptr<Instruction>> {
                    if (const auto fmul1{fsub->get_lhs()->is<FMul>()}) {
                        // (fa * fb) - fc = fmsub(fa, fb, fc)
                        return std::make_optional(
                                FMsub::create("fmsub", fmul1->get_lhs(), fmul1->get_rhs(), fsub->get_rhs(), nullptr));
                    }
                    return std::nullopt;
                }();
                if (new_inst.has_value()) {
                    replace_instruction(fb, new_inst.value(), block, instructions, i);
                    changed = true;
                }
            }
        }
    };

    const auto handle_fnmadd_fnmsub = [&](const std::shared_ptr<Block> &block) -> void {
        auto &instructions{block->get_instructions()};
        for (size_t i{0}; i < instructions.size(); ++i) {
            if (instructions[i]->get_op() == Operator::FLOATBINARY) {
                if (const auto fb{instructions[i]->as<FloatBinary>()}; fb->floatbinary_op() == FloatBinary::Op::SUB) {
                    const auto fsub{fb->as<FSub>()};
                    if (const auto fmul{fsub->get_rhs()->is<FMul>()}) {
                        // fa - (fb * fc) = fnmsub(fb, fc, fa) = -fmsub(fb, fc, fa)
                        const auto fnmsub =
                                FNmsub::create("fnmsub", fmul->get_lhs(), fmul->get_rhs(), fsub->get_lhs(), nullptr);
                        replace_instruction(fb, fnmsub, block, instructions, i);
                        changed = true;
                    }
                }
            } else if (instructions[i]->get_op() == Operator::FNEG) {
                const auto fneg{instructions[i]->as<FNeg>()};
                if (const auto fmadd{fneg->get_value()->is<FMadd>()}) {
                    // -fmadd(fa, fb, fc) = fnmadd(fa, fb, fc)
                    const auto fnmadd =
                            FNmadd::create("fnmadd", fmadd->get_x(), fmadd->get_y(), fmadd->get_z(), nullptr);
                    replace_instruction(fneg, fnmadd, block, instructions, i);
                    changed = true;
                } else if (const auto fmsub{fneg->get_value()->is<FNmsub>()}) {
                    // -fmsub(fa, fb, fc) = fnmsub(fa, fb, fc)
                    const auto fnmsub =
                            FNmsub::create("fnmsub", fmsub->get_x(), fmsub->get_y(), fmsub->get_z(), nullptr);
                    replace_instruction(fneg, fnmsub, block, instructions, i);
                    changed = true;
                }
            }
        }
    };

    do {
        changed = false;
        std::for_each(func->get_blocks().begin(), func->get_blocks().end(), [&](const auto &block) {
            handle_fmadd_fmsub(block);
            handle_fneg(block);
            handle_fnmadd_fnmsub(block);
        });
    } while (changed);
    return changed;
}
} // namespace

namespace Pass {
void AlgebraicSimplify::transform(const std::shared_ptr<Module> module) {
    bool changed = false;
    do {
        changed = false;
        // 对于每一条满足交换律的IntBinary，满足常数均位于运算符右侧
        const auto standardize_binary = create<StandardizeBinary>();
        standardize_binary->run_on(module);
        std::for_each(module->get_functions().begin(), module->get_functions().end(), [&](const auto &func) {
            std::for_each(func->get_blocks().begin(), func->get_blocks().end(), [&](const auto &b) {
                std::for_each(b->get_instructions().begin(), b->get_instructions().end(),
                              [&](const auto &inst) { changed |= GlobalValueNumbering::fold_instruction(inst); });
                changed |= handle_intbinary_icmp(b);
            });
        });
        // std::for_each(module->get_functions().begin(), module->get_functions().end(),
        //               [&](const auto &func) { changed |= handle_float_ternary(func); });
        if (changed) [[likely]] {
            create<DeadInstEliminate>()->run_on(module);
        }
    } while (changed);
    create<DeadInstEliminate>()->run_on(module);
}

void AlgebraicSimplify::transform(const std::shared_ptr<Function> &func) {
    bool changed = false;
    do {
        changed = false;
        // 对于每一条满足交换律的IntBinary，满足常数均位于运算符右侧
        create<StandardizeBinary>()->run_on(func);
        std::for_each(func->get_blocks().begin(), func->get_blocks().end(), [&](const auto &b) {
            std::for_each(b->get_instructions().begin(), b->get_instructions().end(),
                          [&](const auto &inst) { changed |= GlobalValueNumbering::fold_instruction(inst); });
            changed |= handle_intbinary_icmp(b);
        });
        // changed |= handle_float_ternary(func);
        if (changed) [[likely]] {
            create<DeadInstEliminate>()->run_on(func);
        }
    } while (changed);
    create<DeadInstEliminate>()->run_on(func);
}
} // namespace Pass

#include "Mir/Builder.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DCE.h"

using namespace Mir;

namespace {
// 替换指令
void replace_instruction(const std::shared_ptr<Binary> &from, const std::shared_ptr<Value> &to,
                         const std::shared_ptr<Block> &current_block,
                         std::vector<std::shared_ptr<Instruction>> &instructions, const size_t &idx) {
    from->replace_by_new_value(to);
    from->clear_operands();
    if (const auto &target_inst = std::dynamic_pointer_cast<Instruction>(to)) {
        if (target_inst->get_block() != nullptr) { return; }
        target_inst->set_block(current_block, false);
        instructions[idx] = target_inst;
    }
}

// 在idx的位置插入指令
void insert_instruction(const std::shared_ptr<Instruction> &instruction,
                        const std::shared_ptr<Block> &current_block,
                        std::vector<std::shared_ptr<Instruction>> &instructions, size_t &idx) {
    instruction->set_block(current_block, false);
    instructions.insert(instructions.begin() + static_cast<long>(idx), instruction);
    ++idx;
}

[[nodiscard]]
bool reduce_add(const std::shared_ptr<Add> &add, std::vector<std::shared_ptr<Instruction>> &instructions,
                size_t &idx) {
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
        const auto x = mul_lhs->get_lhs(), y = mul_lhs->get_rhs(),
                   z = mul_rhs->get_lhs(), w = mul_rhs->get_rhs();
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
bool reduce_sub(const std::shared_ptr<Sub> &sub, std::vector<std::shared_ptr<Instruction>> &instructions,
                size_t &idx) {
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
        const auto x = mul_lhs->get_lhs(), y = mul_lhs->get_rhs(),
                   z = mul_rhs->get_lhs(), w = mul_rhs->get_rhs();
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
        if (const auto c1 = std::dynamic_pointer_cast<ConstInt>(sub_lhs->get_lhs());
            c1 != nullptr && c1->is_zero()) {
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
bool run_on_block(const std::shared_ptr<Block> &block) {
    auto &instructions = block->get_instructions();
    bool changed = false;
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (instructions[i]->get_op() != Operator::INTBINARY) {
            continue;
        }
        changed |= [&]() -> bool {
            switch (const auto binary{instructions[i]->as<IntBinary>()}; binary->intbinary_op()) {
                case IntBinary::Op::ADD: return reduce_add(binary->as<Add>(), instructions, i);
                case IntBinary::Op::SUB: return reduce_sub(binary->as<Sub>(), instructions, i);
                case IntBinary::Op::MUL: return reduce_mul(binary->as<Mul>(), instructions, i);
                case IntBinary::Op::DIV: return reduce_div(binary->as<Div>(), instructions, i);
                case IntBinary::Op::MOD: return reduce_mod(binary->as<Mod>(), instructions, i);
                case IntBinary::Op::SMAX: return reduce_max(binary->as<Smax>(), instructions, i);
                case IntBinary::Op::SMIN: return reduce_min(binary->as<Smin>(), instructions, i);
                default: return false;
            }
        }();
    }
    return changed;
}
}

void Pass::AlgebraicSimplify::transform(const std::shared_ptr<Module> module) {
    bool changed = false;
    do {
        changed = false;
        // 对于每一条满足交换律的IntBinary，满足常数均位于运算符右侧
        const auto standardize_binary = create<StandardizeBinary>();
        standardize_binary->run_on(module);
        for (const auto &func: *module) {
            for (const auto &block: func->get_blocks()) {
                changed |= run_on_block(block);
            }
        }
        create<DeadInstEliminate>()->run_on(module);
    } while (changed);
    create<DeadInstEliminate>()->run_on(module);
}

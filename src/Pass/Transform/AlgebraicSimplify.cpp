#include "Mir/Builder.h"
#include "Pass/Transform.h"

using namespace Mir;

// 替换指令
static void replace_instruction(const std::shared_ptr<Binary> &from, const std::shared_ptr<Value> &to,
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
static void insert_instruction(const std::shared_ptr<Instruction> &instruction,
                               const std::shared_ptr<Block> &current_block,
                               std::vector<std::shared_ptr<Instruction>> &instructions, size_t &idx) {
    instruction->set_block(current_block, false);
    instructions.insert(instructions.begin() + static_cast<long>(idx), instruction);
    ++idx;
}

static bool reduce_add(const std::shared_ptr<Add> &add, std::vector<std::shared_ptr<Instruction>> &instructions,
                       size_t &idx) {
    const auto current_block = add->get_block();
    const auto lhs = add->get_lhs(), rhs = add->get_rhs();
    // a + a = 2 * a
    if (lhs == rhs) {
        const auto new_mul = Mul::create(Builder::gen_variable_name(), lhs, std::make_shared<ConstInt>(2), nullptr);
        replace_instruction(add, new_mul, current_block, instructions, idx);
        return true;
    }
    if (rhs->is_constant()) {
        // a + 0 = 0
        const auto constant_rhs = std::static_pointer_cast<ConstInt>(rhs);
        if (std::any_cast<int>(constant_rhs->get_constant_value()) == 0) {
            add->replace_by_new_value(lhs);
            return true;
        }
        // (a + c1) + c2 = a + (c1 + c2)
        if (const auto add_lhs = std::dynamic_pointer_cast<Add>(lhs);
            add_lhs != nullptr && add_lhs->get_rhs()->is_constant()) {
            const auto c1 = std::static_pointer_cast<ConstInt>(add_lhs->get_rhs());
            const auto c = std::make_shared<ConstInt>(*c1 + *constant_rhs);
            const auto new_add = Add::create(Builder::gen_variable_name(), add_lhs->get_lhs(), c, nullptr);
            replace_instruction(add, new_add, current_block, instructions, idx);
            return true;
        }
        if (const auto sub_lhs = std::dynamic_pointer_cast<Sub>(lhs)) {
            // (a - c1) + c2 = a + (c2 - c1)
            const auto lhs1 = sub_lhs->get_lhs(), rhs1 = sub_lhs->get_rhs();
            if (!lhs1->is_constant() && rhs1->is_constant()) {
                const auto c1 = std::static_pointer_cast<ConstInt>(rhs1);
                const auto c = std::make_shared<ConstInt>(*constant_rhs - *c1);
                const auto new_add = Add::create(Builder::gen_variable_name(), lhs1, c, nullptr);
                replace_instruction(add, new_add, current_block, instructions, idx);
                return true;
            }
            // (c1 - a) + c2 = (c1 + c2) - a
            if (lhs1->is_constant() && !rhs1->is_constant()) {
                const auto c1 = std::static_pointer_cast<ConstInt>(lhs1);
                const auto c = std::make_shared<ConstInt>(*c1 + *constant_rhs);
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
    // 对于a * b + c * d，有以下几种情况
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
        const auto const_one = std::make_shared<ConstInt>(1);
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
        const auto const_one = std::make_shared<ConstInt>(1);
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

static bool reduce_sub(const std::shared_ptr<Sub> &sub, std::vector<std::shared_ptr<Instruction>> &instructions,
                       size_t &idx) {
    const auto current_block = sub->get_block();
    const auto lhs = sub->get_lhs(), rhs = sub->get_rhs();
    // a - a = 0
    if (lhs == rhs) {
        const auto const_zero = std::make_shared<ConstInt>(0);
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
            const auto new_sub = Sub::create(Builder::gen_variable_name(), std::make_shared<ConstInt>(0), b, nullptr);
            replace_instruction(sub, new_sub, current_block, instructions, idx);
            return true;
        }
        if (lhs == b) {
            const auto new_sub = Sub::create(Builder::gen_variable_name(), std::make_shared<ConstInt>(0), a, nullptr);
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
                const auto c = std::make_shared<ConstInt>(*constant_lhs - *c2);
                const auto new_sub = Sub::create(Builder::gen_variable_name(), c, add_rhs->get_lhs(), nullptr);
                replace_instruction(sub, new_sub, current_block, instructions, idx);
                return true;
            }
        }
        // c1 - (x - c2) = (c1 + c2) - x
        if (const auto sub_rhs = std::dynamic_pointer_cast<Sub>(rhs)) {
            if (const auto c2 = std::dynamic_pointer_cast<ConstInt>(sub_rhs->get_rhs())) {
                const auto c = std::make_shared<ConstInt>(*constant_lhs + *c2);
                const auto new_sub = Sub::create(Builder::gen_variable_name(), c, sub_rhs->get_lhs(), nullptr);
                replace_instruction(sub, new_sub, current_block, instructions, idx);
                return true;
            }
        }
    }
    if (rhs->is_constant()) {}
    return false;
}

static bool reduce_mul(const std::shared_ptr<Mul> &mul, std::vector<std::shared_ptr<Instruction>> &instructions,
                       size_t &idx) {
    return false;
}

static bool reduce_div(const std::shared_ptr<Div> &div, std::vector<std::shared_ptr<Instruction>> &instructions,
                       size_t &idx) {
    return false;
}

static bool reduce_mod(const std::shared_ptr<Mod> &mod, std::vector<std::shared_ptr<Instruction>> &instructions,
                       size_t &idx) {
    return false;
}

[[nodiscard]] static bool run_on_block(const std::shared_ptr<Block> &block) {
    auto &instructions = block->get_instructions();
    bool changed = false;
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (instructions[i]->get_op() != Operator::INTBINARY) {
            continue;
        }
        if (const auto binary_inst = std::static_pointer_cast<IntBinary>(instructions[i]);
            binary_inst->op == IntBinary::Op::ADD) {
            const auto add = std::static_pointer_cast<Add>(binary_inst);
            changed |= reduce_add(add, instructions, i);
        } else if (binary_inst->op == IntBinary::Op::SUB) {
            const auto sub = std::static_pointer_cast<Sub>(binary_inst);
            changed |= reduce_sub(sub, instructions, i);
        } else if (binary_inst->op == IntBinary::Op::MUL) {
            const auto mul = std::static_pointer_cast<Mul>(binary_inst);
            changed |= reduce_mul(mul, instructions, i);
        } else if (binary_inst->op == IntBinary::Op::DIV) {
            const auto div = std::static_pointer_cast<Div>(binary_inst);
            changed |= reduce_div(div, instructions, i);
        } else if (binary_inst->op == IntBinary::Op::MOD) {
            const auto mod = std::static_pointer_cast<Mod>(binary_inst);
            changed |= reduce_mod(mod, instructions, i);
        }
    }
    return changed;
}

void Pass::AlgebraicSimplify::transform(const std::shared_ptr<Module> module) {
    bool changed = false;
    do {
        changed = false;
        // 常量折叠
        const auto fold = create<ConstantFolding>();
        fold->run_on(module);
        // 对于每一条满足交换律的IntBinary，满足常数均位于运算符右侧
        const auto standardize_binary = create<StandardizeBinary>();
        standardize_binary->run_on(module);
        for (const auto &func: *module) {
            for (const auto &block: func->get_blocks()) {
                changed |= run_on_block(block);
            }
        }
        const auto dead_inst = create<DeadInstEliminate>();
        dead_inst->run_on(module);
    } while (changed);
}

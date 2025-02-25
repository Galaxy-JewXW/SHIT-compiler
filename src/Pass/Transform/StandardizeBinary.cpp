#include <Mir/Builder.h>

#include "Pass/Transform.h"

using namespace Mir;

namespace Pass {
// 判断一个指令是否满足交换律，即调换两个操作数的顺序不会改变操作结果
static void try_exchange_operands(const std::shared_ptr<Instruction> &instruction) {
    if (const auto op = instruction->get_op(); op == Operator::INTBINARY) {
        if (const auto int_binary = std::static_pointer_cast<IntBinary>(instruction);
            int_binary->op == IntBinary::Op::ADD || int_binary->op == IntBinary::Op::MUL) {
            if (int_binary->get_lhs()->is_constant() && !int_binary->get_rhs()->is_constant()) {
                std::swap(int_binary->lhs(), int_binary->rhs());
            }
        }
    } else if (op == Operator::FLOATBINARY) {
        if (const auto float_binary = std::static_pointer_cast<FloatBinary>(instruction);
            float_binary->op == FloatBinary::Op::ADD || float_binary->op == FloatBinary::Op::MUL) {
            if (float_binary->get_lhs()->is_constant() && !float_binary->get_rhs()->is_constant()) {
                std::swap(float_binary->lhs(), float_binary->rhs());
            }
        }
    } else if (op == Operator::ICMP) {
        if (const auto &icmp = std::static_pointer_cast<Icmp>(instruction);
            icmp->get_lhs()->is_constant() && !icmp->get_rhs()->is_constant()) {
            std::swap(icmp->lhs(), icmp->rhs());
            icmp->reverse_op();
        }
    } else if (op == Operator::FCMP) {
        if (const auto &fcmp = std::static_pointer_cast<Fcmp>(instruction);
            fcmp->get_lhs()->is_constant() && !fcmp->get_rhs()->is_constant()) {
            std::swap(fcmp->lhs(), fcmp->rhs());
            fcmp->reverse_op();
        }
    }
}

[[maybe_unused]] static void reverse_sign(std::vector<std::shared_ptr<Instruction>> &instructions, size_t &idx,
                         const std::shared_ptr<Block> &current_block) {
    auto replace_instruction = [&](const std::shared_ptr<Instruction> &from, const std::shared_ptr<Value> &to) {
        from->replace_by_new_value(to);
        from->clear_operands();
        if (const auto &target_inst = std::dynamic_pointer_cast<Instruction>(to)) {
            target_inst->set_block(current_block, false);
            instructions[idx] = target_inst;
        }
    };
    const auto binary = std::static_pointer_cast<IntBinary>(instructions[idx]);
    if (!binary->get_rhs()->is_constant()) { return; }
    if (binary->op == IntBinary::Op::ADD) {
        if (const int int_rhs = std::any_cast<int>(
            std::static_pointer_cast<ConstInt>(binary->get_rhs())->get_constant_value()); int_rhs < 0) {
            const auto c = std::make_shared<ConstInt>(-int_rhs);
            const auto new_sub = Sub::create(Builder::gen_variable_name(), binary->get_lhs(), c, nullptr);
            replace_instruction(binary, new_sub);
        }
    } else if (binary->op == IntBinary::Op::SUB) {
        if (const int int_rhs = std::any_cast<int>(
            std::static_pointer_cast<ConstInt>(binary->get_rhs())->get_constant_value()); int_rhs < 0) {
            const auto c = std::make_shared<ConstInt>(-int_rhs);
            const auto new_add = Add::create(Builder::gen_variable_name(), binary->get_lhs(), c, nullptr);
            replace_instruction(binary, new_add);
        }
    }
}

static void run_on_block(const std::shared_ptr<Block> &block) {
    for (const auto &instruction: block->get_instructions()) {
        try_exchange_operands(instruction);
    }
    auto &instructions = block->get_instructions();
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (instructions[i]->get_op() != Operator::INTBINARY) {
            continue;
        }
        reverse_sign(instructions, i, block);
    }
}

void StandardizeBinary::transform(const std::shared_ptr<Module> module) {
    for (const auto &function: *module) {
        for (const auto &block: function->get_blocks()) {
            run_on_block(block);
        }
    }
}
}

#include "Mir/Builder.h"
#include "Mir/Const.h"
#include "Pass/Transforms/Common.h"

using namespace Mir;

namespace {
// 判断一个指令是否满足交换律，即调换两个操作数的顺序不会改变操作结果
void try_exchange_operands(const std::shared_ptr<Instruction> &instruction) {
    if (const auto op = instruction->get_op(); op == Operator::INTBINARY) {
        if (const auto int_binary = std::static_pointer_cast<IntBinary>(instruction);
            int_binary->is_commutative()) {
            if (int_binary->get_lhs()->is_constant() && !int_binary->get_rhs()->is_constant()) {
                int_binary->swap_operands();
            }
        }
    } else if (op == Operator::FLOATBINARY) {
        if (const auto float_binary = std::static_pointer_cast<FloatBinary>(instruction);
            float_binary->is_commutative()) {
            if (float_binary->get_lhs()->is_constant() && !float_binary->get_rhs()->is_constant()) {
                float_binary->swap_operands();
            }
        }
    } else if (op == Operator::ICMP) {
        if (const auto &icmp = std::static_pointer_cast<Icmp>(instruction);
            icmp->get_lhs()->is_constant() && !icmp->get_rhs()->is_constant()) {
            icmp->reverse_op();
        }
    } else if (op == Operator::FCMP) {
        if (const auto &fcmp = std::static_pointer_cast<Fcmp>(instruction);
            fcmp->get_lhs()->is_constant() && !fcmp->get_rhs()->is_constant()) {
            fcmp->reverse_op();
        }
    }
}

void reverse_sign(std::vector<std::shared_ptr<Instruction>> &instructions, const size_t &idx,
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
        if (const int int_rhs = binary->get_rhs()->as<ConstInt>()->get<int>(); int_rhs < 0) {
            const auto c = ConstInt::create(-int_rhs);
            const auto new_sub = Sub::create(Builder::gen_variable_name(), binary->get_lhs(), c, nullptr);
            replace_instruction(binary, new_sub);
        }
    } else if (binary->op == IntBinary::Op::SUB) {
        if (const int int_rhs = binary->get_rhs()->as<ConstInt>()->get<int>(); int_rhs < 0) {
            const auto c = ConstInt::create(-int_rhs);
            const auto new_add = Add::create(Builder::gen_variable_name(), binary->get_lhs(), c, nullptr);
            replace_instruction(binary, new_add);
        }
    }
}

void run_on_block(const std::shared_ptr<Block> &block) {
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
}

namespace Pass {
void StandardizeBinary::transform(const std::shared_ptr<Module> module) {
    for (const auto &function: *module) {
        for (const auto &block: function->get_blocks()) {
            run_on_block(block);
        }
    }
}
}

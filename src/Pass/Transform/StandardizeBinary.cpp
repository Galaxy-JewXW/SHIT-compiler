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

static void run_on_block(const std::shared_ptr<Block> &block) {
    for (const auto &instruction: block->get_instructions()) {
        try_exchange_operands(instruction);
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

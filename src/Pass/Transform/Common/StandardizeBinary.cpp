#include <type_traits>

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
        if (const auto target_inst = to->is<Instruction>()) {
            target_inst->set_block(current_block, false);
            instructions[idx] = target_inst;
        }
    };

    const auto binary = std::static_pointer_cast<IntBinary>(instructions[idx]);
    if (!binary->get_rhs()->is_constant()) {
        return;
    }
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
void handle_cmp(std::vector<std::shared_ptr<Instruction>> &instructions, const size_t &idx,
                const std::shared_ptr<Block> &current_block) {
    const auto cmp{instructions[idx]->as<Compare>()};
    int cnt{0};
    cnt += static_cast<int>(cmp->get_lhs()->is_constant());
    cnt += static_cast<int>(cmp->get_rhs()->is_constant());
    if (cnt != 1) {
        return;
    }

    const auto &lhs{cmp->get_lhs()}, &rhs{cmp->get_rhs()};
    if (lhs->is_constant() || !rhs->is_constant()) {
        log_fatal("Should handle before");
    }
    const auto inst{lhs->template is<typename Trait<Compare>::Binary>()};
    const auto constant_value{rhs->template as<typename Trait<Compare>::ConstantType>()};
    if (inst == nullptr) {
        return;
    }
    if (const auto t{inst->op}; t == Trait<Compare>::Binary::Op::ADD) {
        const auto add_inst{inst->template as<typename Trait<Compare>::AddInst>()};
    } else if (t == Trait<Compare>::Binary::Op::SUB) {
        const auto sub_inst{inst->template as<typename Trait<Compare>::SubInst>()};
    } else if (t == Trait<Compare>::Binary::Op::MUL) {
        const auto mul_inst{inst->template as<typename Trait<Compare>::MulInst>()};
    } else if (t == Trait<Compare>::Binary::Op::DIV) {
        const auto div_inst{inst->template as<typename Trait<Compare>::DivInst>()};
    }
}

void run_on_block(const std::shared_ptr<Block> &block) {
    std::for_each(block->get_instructions().begin(), block->get_instructions().end(), try_exchange_operands);
    auto &instructions = block->get_instructions();
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (const auto t{instructions[i]->get_op()}; t == Operator::INTBINARY) {
            reverse_sign(instructions, i, block);
        } else if (t == Operator::ICMP) {
            handle_cmp<Icmp>(instructions, i, block);
        }
    }
}
}

namespace Pass {
void StandardizeBinary::transform(const std::shared_ptr<Module> module) {
    std::for_each(module->get_functions().begin(), module->get_functions().end(), [&](const auto &func) {
        std::for_each(func->get_blocks().begin(), func->get_blocks().end(), run_on_block);
    });
}
}

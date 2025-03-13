#include "Mir/Interpreter.h"

#include <Mir/Instruction.h>

#include "Utils/Log.h"

namespace Mir {
Interpreter::eval_t Interpreter::get_value(const std::shared_ptr<Value> &value) {
    if (value->is_constant()) {
        if (const auto constant_float = std::dynamic_pointer_cast<ConstFloat>(value)) {
            return std::any_cast<double>(constant_float->get_constant_value());
        }
        if (const auto constant_int = std::dynamic_pointer_cast<ConstInt>(value)) {
            return std::any_cast<int>(constant_int->get_constant_value());
        }
        if (const auto constant_bool = std::dynamic_pointer_cast<ConstBool>(value)) {
            return std::any_cast<int>(constant_bool->get_constant_value());
        }
    }
    if (value_map.find(value) != value_map.end()) {
        return value_map[value];
    }
    log_fatal("Unknown value %s", value->to_string().c_str());
}

void Interpreter::interpret_br(const std::shared_ptr<Branch> &branch) {
    prev_block = current_block;
    if (get_value(branch->get_cond())) {
        current_block = branch->get_true_block();
    } else {
        current_block = branch->get_false_block();
    }
}

void Interpreter::interpret_jump(const std::shared_ptr<Jump> &jump) {
    prev_block = current_block;
    current_block = jump->get_target_block();
}

void Interpreter::interpret_ret(const std::shared_ptr<Ret> &ret) {
    prev_block = current_block;
    current_block = nullptr;
    if (ret->get_type()->is_void()) {
        func_return_value = 0;
    } else {
        func_return_value = get_value(ret->get_value());
    }
}

void Interpreter::interpret_intbinary(const std::shared_ptr<IntBinary> &binary) {
    eval_t res;
    eval_t left = get_value(binary->get_lhs()), right = get_value(binary->get_rhs());
    // switch (binary->op) {
    //     case IntBinary::Op::ADD:
    //         res = left + right;
    // }
}


void Interpreter::interpret_instruction(const std::shared_ptr<Instruction> &instruction) {
    switch (instruction->get_op()) {
        case Operator::BRANCH:
            interpret_br(std::static_pointer_cast<Branch>(instruction));
            break;
        case Operator::JUMP:
            interpret_jump(std::static_pointer_cast<Jump>(instruction));
            break;
        case Operator::RET:
            interpret_ret(std::static_pointer_cast<Ret>(instruction));
            break;
        case Operator::INTBINARY:
            interpret_intbinary(std::static_pointer_cast<IntBinary>(instruction));
            break;
        case Operator::FLOATBINARY:
            interpret_floatbinary(std::static_pointer_cast<FloatBinary>(instruction));
            break;
        default:
            log_error("Unhandled instruction type");
    }
}


Interpreter::eval_t Interpreter::interpret_function(const std::shared_ptr<Function> &func,
                                                    const std::vector<eval_t> &real_args) {
    const auto &arguments = func->get_arguments();
    if (arguments.size() != real_args.size()) {
        log_error("Wrong number of arguments");
    }
    for (size_t i = 0; i < arguments.size(); i++) {
        value_map[arguments[i]] = real_args[i];
    }
    prev_block = nullptr;
    current_block = func->get_blocks().front();
    while (current_block != nullptr) {
        for (const auto &instruction: current_block->get_instructions()) {
            interpret_instruction(instruction);
        }
    }
    return func_return_value;
}
}

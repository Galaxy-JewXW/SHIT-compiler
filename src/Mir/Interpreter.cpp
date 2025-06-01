#include "Mir/Interpreter.h"
#include "Mir/Const.h"
#include "Mir/Instruction.h"

using namespace Mir;

void Interpreter::abort() {
    throw std::runtime_error("Interpreter abort");
}

eval_t Interpreter::get_runtime_value(Value *const value) const {
    if (value->is_constant()) {
        return dynamic_cast<Const *>(value)->get_constant_value();
    }
    if (frame->value_map.find(value) != frame->value_map.end()) {
        return frame->value_map[value];
    }
    log_error("%s cannot convert to an eval_t type", value->to_string().c_str());
}

void Interpreter::interpret_instruction(const std::shared_ptr<Instruction> &instruction) {
    if (counter >= counter_limit) [[unlikely]] {
        abort();
    }
    ++counter;
    instruction->do_interpret(this);
}

void Interpreter::interpret_function(const std::shared_ptr<Function> &func, const std::vector<eval_t> &real_args) {
    const auto &arguments{func->get_arguments()};
    if (arguments.size() != real_args.size()) {
        log_error("Wrong number of arguments");
    }
    frame = std::make_shared<Frame>();
    for (size_t i{0}; i < arguments.size(); i++) {
        frame->value_map[arguments[i].get()] = real_args[i];
    }
    frame->prev_block = nullptr;
    frame->current_block = func->get_blocks().front();
    while (frame->current_block != nullptr) {
        const auto original_block = frame->current_block;
        const auto &instructions = original_block->get_instructions();
        for (size_t i{0}; i < instructions.size(); ++i) {
            if (const auto &instruction = instructions[i];
                instruction->get_op() == Operator::PHI) {
                interpret_instruction(instruction->as<Phi>());
                if (i + 1 < instructions.size() && instructions[i + 1]->get_op() == Operator::PHI) {
                    continue;
                }
                for (const auto &[phi, eval_value]: frame->phi_cache) {
                    frame->value_map[phi] = eval_value;
                }
                frame->phi_cache.clear();
            } else {
                interpret_instruction(instruction);
            }
            // 如果当前块已改变，终止处理后续指令
            if (frame->current_block != original_block) {
                break;
            }
        }
    }
}

void Fptosi::do_interpret(Interpreter *const interpreter) {
    interpreter->frame->value_map[this] = interpreter->get_runtime_value(this->get_value()).get<int>();
}

void Sitofp::do_interpret(Interpreter *const interpreter) {
    interpreter->frame->value_map[this] = interpreter->get_runtime_value(this->get_value()).get<double>();
}

void Fcmp::do_interpret(Interpreter *const interpreter) {
    const eval_t left{interpreter->get_runtime_value(this->get_lhs())},
                 right{interpreter->get_runtime_value(this->get_rhs())};
    const eval_t res = std::visit([&](const eval_t &l, const eval_t &r) -> eval_t {
        switch (this->op) {
            case Op::EQ: return l == r;
            case Op::NE: return l != r;
            case Op::GT: return l > r;
            case Op::GE: return l >= r;
            case Op::LT: return l < r;
            case Op::LE: return l <= r;
            default: log_error("Unhandled binary operator");
        }
    }, left, right);
    interpreter->frame->value_map[this] = res;
}

void Icmp::do_interpret(Interpreter *const interpreter) {
    const eval_t left{interpreter->get_runtime_value(this->get_lhs())},
                 right{interpreter->get_runtime_value(this->get_rhs())};
    const eval_t res = std::visit([&](const eval_t &l, const eval_t &r) -> eval_t {
        switch (this->op) {
            case Op::EQ: return l == r;
            case Op::NE: return l != r;
            case Op::GT: return l > r;
            case Op::GE: return l >= r;
            case Op::LT: return l < r;
            case Op::LE: return l <= r;
            default: log_error("Unhandled binary operator");
        }
    }, left, right);
    interpreter->frame->value_map[this] = res;
}

void Zext::do_interpret(Interpreter *const interpreter) {
    interpreter->frame->value_map[this] = interpreter->get_runtime_value(this->get_value()).get<int>();
}

void Branch::do_interpret(Interpreter *const interpreter) {
    interpreter->frame->prev_block = interpreter->frame->current_block;
    interpreter->frame->current_block = interpreter->get_runtime_value(this->get_cond()).get<int>()
                                            ? this->get_true_block()
                                            : this->get_false_block();
}

void Jump::do_interpret(Interpreter *const interpreter) {
    interpreter->frame->prev_block = interpreter->frame->current_block;
    interpreter->frame->current_block = this->get_target_block();
}

void Ret::do_interpret(Interpreter *const interpreter) {
    interpreter->frame->prev_block = interpreter->frame->current_block;
    interpreter->frame->current_block = nullptr;
    interpreter->frame->ret_value = this->get_type()->is_void() ? 0 : interpreter->get_runtime_value(this->get_value());
}

void Call::do_interpret(Interpreter *const interpreter) {
    std::vector<eval_t> real_args;
    real_args.reserve(this->get_params().size());
    for (size_t i{0}; i < this->get_params().size(); ++i) {
        real_args.push_back(interpreter->get_runtime_value(this->get_params()[i]));
    }
    const auto called_func{this->get_function()->as<Function>()};
    if (called_func->is_runtime_func()) {
        log_error("Unhandled runtime function: %s", called_func->get_name().c_str());
    }
    eval_t sub_ret_value;
    if (const Interpreter::Key key{called_func->get_name(), real_args}; interpreter->cache.lock()->contains(key)) {
        sub_ret_value = interpreter->cache.lock()->get(key);
    } else {
        const auto prev_frame{interpreter->frame};
        interpreter->interpret_function(called_func, real_args);
        sub_ret_value = interpreter->frame->ret_value;
        interpreter->frame = prev_frame;
        interpreter->cache.lock()->put(key, sub_ret_value);
    }
    if (!this->get_type()->is_void()) {
        interpreter->frame->value_map[this] = sub_ret_value;
    }
}

void Phi::do_interpret(Interpreter *const interpreter) {
    interpreter->frame->phi_cache[this] = interpreter->get_runtime_value(
        this->optional_values.at(interpreter->frame->prev_block));
}

#define BINARY_DO_INTERPRET(Class, Op, Type) \
void Class::do_interpret(Interpreter *const interpreter)  { \
    const eval_t left{interpreter->get_runtime_value(this->get_lhs())}, \
                 right{interpreter->get_runtime_value(this->get_rhs())}; \
    interpreter->frame->value_map[this] = left.get<Type>() Op right.get<Type>(); \
}

BINARY_DO_INTERPRET(Add, +, int)
BINARY_DO_INTERPRET(Sub, -, int)
BINARY_DO_INTERPRET(Mul, *, int)
BINARY_DO_INTERPRET(Div, /, int)
BINARY_DO_INTERPRET(Mod, %, int)
BINARY_DO_INTERPRET(FAdd, +, double)
BINARY_DO_INTERPRET(FSub, -, double)
BINARY_DO_INTERPRET(FMul, *, double)
BINARY_DO_INTERPRET(FDiv, /, double)

#undef BINARY_DO_INTERPRET

void FMod::do_interpret(Interpreter *const interpreter) {
    const eval_t left{interpreter->get_runtime_value(this->get_lhs())},
                 right{interpreter->get_runtime_value(this->get_rhs())};
    interpreter->frame->value_map[this] = std::fmod(left.get<double>(), right.get<double>());
}

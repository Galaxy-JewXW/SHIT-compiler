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

void Fptosi::do_interpret(const Interpreter *const interpreter) const {
    interpreter->frame->value_map[this] = interpreter->get_runtime_value(this->get_value()).get<int>();
}

void Sitofp::do_interpret(const Interpreter *const interpreter) const {
    interpreter->frame->value_map[this] = interpreter->get_runtime_value(this->get_value()).get<double>();
}

void Fcmp::do_interpret(const Interpreter *const interpreter) const {
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

void Icmp::do_interpret(const Interpreter *const interpreter) const {
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

void Zext::do_interpret(const Interpreter *const interpreter) const {
    interpreter->frame->value_map[this] = interpreter->get_runtime_value(this->get_value()).get<int>();
}

void Branch::do_interpret(const Interpreter *const interpreter) const {
    interpreter->frame->prev_block = interpreter->frame->current_block;
    interpreter->frame->current_block = interpreter->get_runtime_value(this->get_cond()).get<int>()
                                            ? this->get_true_block()
                                            : this->get_false_block();
}

void Jump::do_interpret(const Interpreter *interpreter) const {
    interpreter->frame->prev_block = interpreter->frame->current_block;
    interpreter->frame->current_block = this->get_target_block();
}

void Ret::do_interpret(const Interpreter *interpreter) const {
    interpreter->frame->prev_block = interpreter->frame->current_block;
    interpreter->frame->current_block = nullptr;
    interpreter->frame->ret_value = this->get_type()->is_void() ? 0 : interpreter->get_runtime_value(this->get_value());
}

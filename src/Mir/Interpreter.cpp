#include "Mir/Interpreter.h"
#include "Mir/Const.h"
#include "Mir/Instruction.h"

using namespace Mir;

namespace {
size_t size_of_type(const std::shared_ptr<Type::Type> &type) {
    if (type->is_int32()) {
        return sizeof(int);
    }
    if (type->is_float()) {
        return sizeof(double);
    }
    if (type->is_array()) {
        return size_of_type(type->as<Type::Array>()->get_atomic_type()) * type->as<Type::Array>()->get_flattened_size();
    }
    log_error("Invalid type %s", type->to_string().c_str());
}
}

size_t Interpreter::counter_limit = 20000;

void Interpreter::abort() { throw std::runtime_error("Interpreter abort"); }

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

void Interpreter::interpret_module_mode(const std::shared_ptr<Function> &main_func) {
    if (!module_mode)
        return;

    frame = std::make_shared<Frame>();
    frame->prev_block = nullptr;
    frame->current_block = main_func->get_blocks().front();
    while (frame->current_block != nullptr) {
        const auto original_block = frame->current_block;
        const auto &instructions = original_block->get_instructions();
        for (size_t i{0}; i < instructions.size(); ++i) {
            if (const auto &instruction = instructions[i]; instruction->get_op() == Operator::PHI) {
                interpret_instruction(instruction->as<Phi>());
                if (i + 1 < instructions.size() && instructions[i + 1]->get_op() == Operator::PHI) {
                    continue;
                }
                for (const auto &[phi, eval_value]: frame->phi_cache) {
                    frame->value_map[phi] = eval_value;
                }
                frame->phi_cache.clear();
            } else if (instruction->get_op() == Operator::ALLOC) {
                interpret_instruction(instruction->as<Alloc>());
                if (i + 2 < instructions.size()
                    && instructions[i + 1]->get_op() == Operator::BITCAST
                    && instructions[i + 2]->get_op() == Operator::CALL) {
                    i = i + 2;
                    continue;
                }
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

void Alloc::do_interpret(Interpreter *interpreter) {
    if (!interpreter->is_module_mode())
        abort();

    const auto array_ty = get_type()->as<Type::Pointer>()->get_contain_type()->as<Type::Array>();
    const auto size = size_of_type(array_ty);
    const auto base = array_ty->get_atomic_type()->is_float() ? sizeof(double) : sizeof(int);
    const auto ad = interpreter->frame->memory.allocate(size, base);
    interpreter->frame->memory.zero_fill(ad, size);
    interpreter->frame->value_map[this] = static_cast<int>(ad);
}

void Load::do_interpret(Interpreter *interpreter) {
    if (!interpreter->is_module_mode())
        abort();

    const auto base = get_type()->is_float() ? sizeof(double) : sizeof(int);
    const auto addr = interpreter->get_runtime_value(get_addr()).get<int>();
    const auto ad = static_cast<size_t>(addr);
    if (base == sizeof(int)) {
        interpreter->frame->value_map[this] = interpreter->frame->memory.read<int>(ad);
    } else {
        interpreter->frame->value_map[this] = interpreter->frame->memory.read<double>(ad);
    }
}

void Store::do_interpret(Interpreter *interpreter) {
    if (!interpreter->is_module_mode())
        abort();

    const auto base = get_value()->get_type()->is_float() ? sizeof(double) : sizeof(int);
    const auto addr = static_cast<size_t>(interpreter->get_runtime_value(get_addr()).get<int>());
    const auto _value = interpreter->get_runtime_value(get_value());
    if (base == sizeof(int)) {
        interpreter->frame->memory.write(addr, _value.get<int>());
    } else {
        interpreter->frame->memory.write(addr, _value.get<double>());
    }
}

void GetElementPtr::do_interpret(Interpreter *interpreter) {
    if (!interpreter->is_module_mode())
        abort();

    const auto base_addr = static_cast<size_t>(interpreter->get_runtime_value(get_addr()).get<int>());
    const auto index = static_cast<size_t>(interpreter->get_runtime_value(get_index()).get<int>());
    const auto element_ty = get_type()->as<Type::Pointer>()->get_contain_type();
    const auto element_size = size_of_type(element_ty);
    if (element_size == 0) {
        abort();
    }

    const size_t new_address = base_addr + index * element_size;
    interpreter->frame->value_map[this] = static_cast<int>(new_address);
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
            if (const auto &instruction = instructions[i]; instruction->get_op() == Operator::PHI) {
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
    const eval_t res = std::visit(
            [&](const eval_t &l, const eval_t &r) -> eval_t {
                switch (this->op) {
                    case Op::EQ:
                        return l == r;
                    case Op::NE:
                        return l != r;
                    case Op::GT:
                        return l > r;
                    case Op::GE:
                        return l >= r;
                    case Op::LT:
                        return l < r;
                    case Op::LE:
                        return l <= r;
                    default:
                        log_error("Unhandled binary operator");
                }
            },
            left, right);
    interpreter->frame->value_map[this] = res;
}

void Icmp::do_interpret(Interpreter *const interpreter) {
    const eval_t left{interpreter->get_runtime_value(this->get_lhs())},
            right{interpreter->get_runtime_value(this->get_rhs())};
    const eval_t res = std::visit(
            [&](const eval_t &l, const eval_t &r) -> eval_t {
                switch (this->op) {
                    case Op::EQ:
                        return l == r;
                    case Op::NE:
                        return l != r;
                    case Op::GT:
                        return l > r;
                    case Op::GE:
                        return l >= r;
                    case Op::LT:
                        return l < r;
                    case Op::LE:
                        return l <= r;
                    default:
                        log_error("Unhandled binary operator");
                }
            },
            left, right);
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
    interpreter->frame->ret_value = operands_.empty() ? 0 : interpreter->get_runtime_value(this->get_value());
}

void Switch::do_interpret(Interpreter *const interpreter) {
    std::unordered_map<eval_t, std::shared_ptr<Block>> _cases;
    for (const auto &[value, block]: cases_table) {
        _cases[interpreter->get_runtime_value(value)] = block;
    }
    interpreter->frame->prev_block = interpreter->frame->current_block;
    if (const auto base{interpreter->get_runtime_value(get_base())}; _cases.find(base) != _cases.end()) {
        interpreter->frame->current_block = _cases[base];
    } else {
        interpreter->frame->current_block = get_default_block();
    }
}

void Call::do_interpret(Interpreter *const interpreter) {
    if (interpreter->is_module_mode()) {
        const auto called_func = get_function()->as<Function>();
        const auto &name = called_func->get_name();
        if (name.find("sysy") != std::string::npos) {
            interpreter->frame->kept.push_back(create(called_func, {get_operands()[1]}, nullptr));
            return;
        }
        if (name == "putint") {
            const auto call = create("putint.l", called_func,
                {ConstInt::create(interpreter->get_runtime_value(get_operands()[1]).get<int>())}, nullptr);
            interpreter->frame->kept.push_back(call);
        } else if (name == "putch") {
            const auto call = create("putch.l", called_func,
                            {ConstInt::create(interpreter->get_runtime_value(get_operands()[1]).get<int>())}, nullptr);
            interpreter->frame->kept.push_back(call);
        } else {
            abort();
        }
        return;
    }

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
    interpreter->frame->phi_cache[this] =
            interpreter->get_runtime_value(this->optional_values.at(interpreter->frame->prev_block));
}

#define BINARY_DO_INTERPRET(Class, Calc, Type)                                                                         \
    void Class::do_interpret(Interpreter *const interpreter) {                                                         \
        const eval_t left{interpreter->get_runtime_value(this->get_lhs())},                                            \
                right{interpreter->get_runtime_value(this->get_rhs())};                                                \
        interpreter->frame->value_map[this] = [](const Type a, const Type b) { Calc }(left.get<Type>(),                \
                                                                                      right.get<Type>());              \
    }

#define TERNARY_DO_INTERPRET(Class, Calc, Type)                                                                        \
    void Class::do_interpret(Interpreter *const interpreter) {                                                         \
        const eval_t x{interpreter->get_runtime_value(this->get_x())},                                                 \
                y{interpreter->get_runtime_value(this->get_y())}, z{interpreter->get_runtime_value(this->get_z())};    \
        interpreter->frame->value_map[this] = [](const int x, const Type y, const Type z) {                            \
            Calc                                                                                                       \
        }(x.get<Type>(), y.get<Type>(), z.get<Type>());                                                                \
    }

// 整数运算
BINARY_DO_INTERPRET(Add, return a + b;, int)
BINARY_DO_INTERPRET(Sub, return a - b;, int)
BINARY_DO_INTERPRET(Mul, return a * b;, int)
BINARY_DO_INTERPRET(Div, return a / b;, int)
BINARY_DO_INTERPRET(Mod, return a % b;, int)
BINARY_DO_INTERPRET(And, return a & b;, int)
BINARY_DO_INTERPRET(Or, return a | b;, int)
BINARY_DO_INTERPRET(Xor, return a ^ b;, int)
BINARY_DO_INTERPRET(Smax, return std::max(a, b);, int)
BINARY_DO_INTERPRET(Smin, return std::min(a, b);, int)

// 浮点数运算
BINARY_DO_INTERPRET(FAdd, return a + b;, double)
BINARY_DO_INTERPRET(FSub, return a - b;, double)
BINARY_DO_INTERPRET(FMul, return a * b;, double)
BINARY_DO_INTERPRET(FDiv, return a / b;, double)
BINARY_DO_INTERPRET(FMod, return std::fmod(a, b);, double)
BINARY_DO_INTERPRET(FSmax, return std::max(a, b);, double)
BINARY_DO_INTERPRET(FSmin, return std::min(a, b);, double)

TERNARY_DO_INTERPRET(FMadd, return x * y + z;, double)
TERNARY_DO_INTERPRET(FNmadd, return -(x * y + z);, double)
TERNARY_DO_INTERPRET(FMsub, return x * y - z;, double)
TERNARY_DO_INTERPRET(FNmsub, return -(x * y - z);, double)

void FNeg::do_interpret(Interpreter *interpreter) {
    interpreter->frame->value_map[this] = -interpreter->get_runtime_value(this->get_value());
}


#undef BINARY_DO_INTERPRET
#undef TERNARY_DO_INTERPRET

void Select::do_interpret(Interpreter *interpreter) {
    const eval_t condition{interpreter->get_runtime_value(this->get_cond())};
    interpreter->frame->value_map[this] = condition.get<int>()
                                                  ? interpreter->get_runtime_value(this->get_true_value())
                                                  : interpreter->get_runtime_value(this->get_false_value());
}

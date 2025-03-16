#include <functional>

#include "Mir/Interpreter.h"
#include "Mir/Instruction.h"
#include "Utils/Log.h"

static size_t type_size(const std::shared_ptr<Mir::Type::Type> &type) {
    if (type->is_float() || type->is_pointer()) {
        return 4;
    }
    if (type->is_integer()) {
        return (7 + std::static_pointer_cast<Mir::Type::Integer>(type)->operator->()) / 8;
    }
    if (type->is_array()) {
        return type_size(std::static_pointer_cast<Mir::Type::Array>(type)->get_atomic_type()) *
               std::static_pointer_cast<Mir::Type::Array>(type)->get_flattened_size();
    }
    return 0;
}

namespace Mir {
eval_t ConstexprFuncInterpreter::get_runtime_value(const std::shared_ptr<Value> &value) {
    if (value->is_constant()) {
        if (const auto constant_float = std::dynamic_pointer_cast<ConstFloat>(value)) {
            return **constant_float;
        }
        if (const auto constant_int = std::dynamic_pointer_cast<ConstInt>(value)) {
            return **constant_int;
        }
        if (const auto constant_bool = std::dynamic_pointer_cast<ConstBool>(value)) {
            return **constant_bool;
        }
    }
    if (value_map.find(value) != value_map.end()) {
        return value_map[value];
    }
    log_fatal("Unknown value %s", value->to_string().c_str());
}

void ConstexprFuncInterpreter::interpret_alloc(const std::shared_ptr<Alloc> &alloc) {
    const eval_t addr = static_cast<int>(eval_stack.size());
    value_map[alloc] = addr;
    const auto type = std::static_pointer_cast<Type::Pointer>(alloc->get_type())->get_contain_type();
    eval_stack.resize(eval_stack.size() + type_size(type) / 4);
}

void ConstexprFuncInterpreter::interpret_load(const std::shared_ptr<Load> &load) {
    const auto addr = static_cast<size_t>(get_runtime_value(load->get_addr()));
    const eval_t res = eval_stack.at(addr);
    value_map[load] = res;
}

void ConstexprFuncInterpreter::interpret_store(const std::shared_ptr<Store> &store) {
    const auto addr = static_cast<size_t>(get_runtime_value(store->get_addr()));
    eval_stack.at(addr) = get_runtime_value(store->get_value());
}

void ConstexprFuncInterpreter::interpret_gep(const std::shared_ptr<GetElementPtr> &gep) {
    const auto base_addr = get_runtime_value(gep->get_addr());
    const auto index = get_runtime_value(gep->get_index());
    const auto contain_type = std::static_pointer_cast<Type::Pointer>(gep->get_addr()->get_type())->get_contain_type();
    if (const auto op_size = gep->get_operands().size(); op_size == 2) {
        // 形如 %1 = getelementptr inbounds [9 x [3 x i32]], [9 x [3 x i32]]* %0, i32 1
        const auto element_size = type_size(contain_type);
        value_map[gep] = base_addr + index * static_cast<eval_t>(element_size);
    } else if (op_size == 3) {
        // 形如 %2 = getelementptr inbounds [9 x [3 x i32]], [9 x [3 x i32]]* %1, i32 0, i32 7
        if (!contain_type->is_array()) {
            log_fatal("Invalid indexing: %s", gep->to_string().c_str());
        }
        const auto array_type = std::static_pointer_cast<Type::Array>(contain_type);
        const auto element_type = array_type->get_element_type();
        const auto element_size = type_size(element_type);
        value_map[gep] = base_addr + index * static_cast<eval_t>(element_size);
    } else {
        log_fatal("Unhandled getelementptr instruction: %s", gep->to_string().c_str());
    }
}

void ConstexprFuncInterpreter::interpret_bitcast(const std::shared_ptr<BitCast> &bitcast) {
    value_map[bitcast] = get_runtime_value(bitcast->get_value());
}

void ConstexprFuncInterpreter::interpret_fptosi(const std::shared_ptr<Fptosi> &fptosi) {
    const eval_t res = get_runtime_value(fptosi->get_value());
    value_map[fptosi] = static_cast<int>(res);
}

void ConstexprFuncInterpreter::interpret_sitofp(const std::shared_ptr<Sitofp> &sitofp) {
    const eval_t res = get_runtime_value(sitofp->get_value());
    value_map[sitofp] = static_cast<double>(res);
}

void ConstexprFuncInterpreter::interpret_fcmp(const std::shared_ptr<Fcmp> &fcmp) {
    const eval_t left = get_runtime_value(fcmp->get_lhs()), right = get_runtime_value(fcmp->get_rhs());
    const eval_t res = std::visit([&fcmp](const eval_t &l, const eval_t &r) -> eval_t {
        switch (fcmp->op) {
            case Fcmp::Op::EQ: return l == r;
            case Fcmp::Op::NE: return l != r;
            case Fcmp::Op::GT: return l > r;
            case Fcmp::Op::GE: return l >= r;
            case Fcmp::Op::LT: return l < r;
            case Fcmp::Op::LE: return l <= r;
            default: log_error("Unhandled binary operator");
        }
    }, left, right);
    value_map[fcmp] = res;
}

void ConstexprFuncInterpreter::interpret_icmp(const std::shared_ptr<Icmp> &icmp) {
    const eval_t left = get_runtime_value(icmp->get_lhs()), right = get_runtime_value(icmp->get_rhs());
    const eval_t res = std::visit([&icmp](const eval_t &l, const eval_t &r) -> eval_t {
        switch (icmp->op) {
            case Icmp::Op::EQ: return l == r;
            case Icmp::Op::NE: return l != r;
            case Icmp::Op::GT: return l > r;
            case Icmp::Op::GE: return l >= r;
            case Icmp::Op::LT: return l < r;
            case Icmp::Op::LE: return l <= r;
            default: log_error("Unhandled binary operator");
        }
    }, left, right);
    value_map[icmp] = res;
}

void ConstexprFuncInterpreter::interpret_zext(const std::shared_ptr<Zext> &zext) {
    value_map[zext] = get_runtime_value(zext->get_value());
}

void ConstexprFuncInterpreter::interpret_br(const std::shared_ptr<Branch> &branch) {
    prev_block = current_block;
    if (get_runtime_value(branch->get_cond())) {
        current_block = branch->get_true_block();
    } else {
        current_block = branch->get_false_block();
    }
}

void ConstexprFuncInterpreter::interpret_jump(const std::shared_ptr<Jump> &jump) {
    prev_block = current_block;
    current_block = jump->get_target_block();
}

void ConstexprFuncInterpreter::interpret_ret(const std::shared_ptr<Ret> &ret) {
    prev_block = current_block;
    current_block = nullptr;
    if (ret->get_type()->is_void()) {
        func_return_value = 0;
    } else {
        func_return_value = get_runtime_value(ret->get_value());
    }
}

void ConstexprFuncInterpreter::interpret_call(const std::shared_ptr<Call> &call) {
    std::vector<eval_t> real_args;
    real_args.reserve(call->get_params().size());
    for (size_t i = 0; i < call->get_params().size(); ++i) {
        real_args.push_back(get_runtime_value(call->get_params()[i]));
    }
    const auto called_func = std::static_pointer_cast<Function>(call->get_function());
    if (called_func->is_runtime_func()) {
        log_error("Unhandled runtime function: %s", called_func->get_name().c_str());
    }
    const auto sub_interpreter = std::make_unique<ConstexprFuncInterpreter>();
    const eval_t sub_return_value = sub_interpreter->interpret_function(called_func, real_args);
    if (!call->get_name().empty()) {
        value_map[call] = sub_return_value;
    }
}

void ConstexprFuncInterpreter::interpret_intbinary(const std::shared_ptr<IntBinary> &binary) {
    const eval_t left = get_runtime_value(binary->get_lhs()), right = get_runtime_value(binary->get_rhs());
    const eval_t res = std::visit([&binary](const eval_t &l, const eval_t &r) -> eval_t {
        switch (binary->op) {
            case IntBinary::Op::ADD: return l + r;
            case IntBinary::Op::SUB: return l - r;
            case IntBinary::Op::MUL: return l * r;
            case IntBinary::Op::DIV: return l / r;
            case IntBinary::Op::MOD: return l % r;
            default: log_error("Unhandled binary operator");
        }
    }, left, right);
    value_map[binary] = res;
}

void ConstexprFuncInterpreter::interpret_floatbinary(const std::shared_ptr<FloatBinary> &binary) {
    eval_t left = get_runtime_value(binary->get_lhs()), right = get_runtime_value(binary->get_rhs());
    const eval_t res = std::visit([&binary](const eval_t &l, const eval_t &r) -> eval_t {
        switch (binary->op) {
            case FloatBinary::Op::ADD: return l + r;
            case FloatBinary::Op::SUB: return l - r;
            case FloatBinary::Op::MUL: return l * r;
            case FloatBinary::Op::DIV: return l / r;
            case FloatBinary::Op::MOD: return l % r;
            default: log_error("Unhandled binary operator");
        }
    }, left, right);
    value_map[binary] = res;
}

void ConstexprFuncInterpreter::interpret_phi(const std::shared_ptr<Phi> &phi) {
    phi_cache[phi] = get_runtime_value(phi->get_optional_values()[prev_block]);
}

void ConstexprFuncInterpreter::interpret_instruction(const std::shared_ptr<Instruction> &instruction) {
    using Handler = std::function<void(ConstexprFuncInterpreter *, std::shared_ptr<Instruction>)>;
    static const std::unordered_map<Operator, Handler> handlers = {
        {
            Operator::ALLOC, [](auto *self, const auto &instr) {
                self->interpret_alloc(std::static_pointer_cast<Alloc>(instr));
            }
        },
        {
            Operator::LOAD, [](auto *self, const auto &instr) {
                self->interpret_load(std::static_pointer_cast<Load>(instr));
            }
        },
        {
            Operator::STORE, [](auto *self, const auto &instr) {
                self->interpret_store(std::static_pointer_cast<Store>(instr));
            }
        },
        {
            Operator::GEP, [](auto *self, const auto &instr) {
                self->interpret_gep(std::static_pointer_cast<GetElementPtr>(instr));
            }
        },
        {
            Operator::BITCAST, [](auto *self, const auto &instr) {
                self->interpret_bitcast(std::static_pointer_cast<BitCast>(instr));
            }
        },
        {
            Operator::FPTOSI, [](auto *self, const auto &instr) {
                self->interpret_fptosi(std::static_pointer_cast<Fptosi>(instr));
            }
        },
        {
            Operator::SITOFP, [](auto *self, const auto &instr) {
                self->interpret_sitofp(std::static_pointer_cast<Sitofp>(instr));
            }
        },
        {
            Operator::FCMP, [](auto *self, const auto &instr) {
                self->interpret_fcmp(std::static_pointer_cast<Fcmp>(instr));
            }
        },
        {
            Operator::ICMP, [](auto *self, const auto &instr) {
                self->interpret_icmp(std::static_pointer_cast<Icmp>(instr));
            }
        },
        {
            Operator::ZEXT, [](auto *self, const auto &instr) {
                self->interpret_zext(std::static_pointer_cast<Zext>(instr));
            }
        },
        {
            Operator::BRANCH, [](auto *self, const auto &instr) {
                self->interpret_br(std::static_pointer_cast<Branch>(instr));
            }
        },
        {
            Operator::JUMP, [](auto *self, const auto &instr) {
                self->interpret_jump(std::static_pointer_cast<Jump>(instr));
            }
        },
        {
            Operator::RET, [](auto *self, const auto &instr) {
                self->interpret_ret(std::static_pointer_cast<Ret>(instr));
            }
        },
        {
            Operator::CALL, [](auto *self, const auto &instr) {
                self->interpret_call(std::static_pointer_cast<Call>(instr));
            }
        },
        {
            Operator::INTBINARY, [](auto *self, const auto &instr) {
                self->interpret_intbinary(std::static_pointer_cast<IntBinary>(instr));
            }
        },
        {
            Operator::FLOATBINARY, [](auto *self, const auto &instr) {
                self->interpret_floatbinary(std::static_pointer_cast<FloatBinary>(instr));
            }
        }
    };
    if (const auto it = handlers.find(instruction->get_op()); it != handlers.end()) {
        it->second(this, instruction);
    } else {
        log_error("Unhandled instruction type: %d", static_cast<int>(instruction->get_op()));
    }
}

eval_t ConstexprFuncInterpreter::interpret_function(const std::shared_ptr<Function> &func,
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
        for (size_t i = 0; i < current_block->get_instructions().size(); ++i) {
            const auto &instruction = current_block->get_instructions()[i];
            if (instruction->get_op() == Operator::PHI) {
                interpret_phi(std::static_pointer_cast<Phi>(instruction));
                if (current_block->get_instructions()[i + 1]->get_op() == Operator::PHI) [[unlikely]] {
                    continue;
                }
                for (const auto &[phi, eval_value]: phi_cache) {
                    value_map[phi] = eval_value;
                }
                phi_cache.clear();
                continue;
            }
            interpret_instruction(instruction);
        }
    }
    return func_return_value;
}
}

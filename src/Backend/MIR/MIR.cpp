#include "Backend/MIR/MIR.h"
#include "Backend/MIR/Instructions.h"

std::shared_ptr<Backend::MIR::Variable> Backend::MIR::Module::find_variable(const std::string &name, const std::shared_ptr<Backend::MIR::Function> &function) {
    if (function->variables.find(name) != function->variables.end()) {
        return function->variables[name];
    }
    for (const std::shared_ptr<Backend::DataSection::Variable> &var : global_data->global_variables) {
        if (var->name == name) {
            return std::make_shared<Backend::MIR::FunctionVariable>(*var);
        }
    }
    return nullptr;
}

std::shared_ptr<Backend::MIR::Value> Backend::MIR::Module::find_value(const std::shared_ptr<Mir::Value> &value, const std::shared_ptr<Backend::MIR::Function> &function) {
    const  std::string &name = value->get_name();
    if (value->is_constant()) {
        return std::make_shared<Backend::MIR::Constant>(std::stoll(name));
    }
    return find_variable(name, function);
}

void Backend::MIR::Module::load_instructions(const std::shared_ptr<Mir::Function> &llvm_function, std::shared_ptr<Backend::MIR::Function> &mir_function) {
    for (const std::shared_ptr<Mir::Argument> &arg : llvm_function->get_arguments()) {
        std::shared_ptr<Backend::MIR::Variable> var = std::make_shared<Backend::MIR::FunctionVariable>(arg->get_name(), Backend::Utils::llvm_to_riscv(*arg->get_type()));
        mir_function->add_variable(var);
    }
    for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
        std::shared_ptr<Backend::MIR::Block> mir_block = mir_function->blocks[llvm_block->get_name()];
        for (const std::shared_ptr<Mir::Instruction> &llvm_instruction : llvm_block->get_instructions()) {
            if (llvm_instruction->get_op() == Mir::Operator::ALLOC) {
                std::shared_ptr<Mir::Alloc> alloc = std::dynamic_pointer_cast<Mir::Alloc>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Variable> var = std::make_shared<Backend::MIR::FunctionVariable>(alloc->get_name(), Backend::Utils::llvm_to_riscv(*alloc->get_type()));
                mir_function->add_variable(var);
            }
        }
        for (const std::shared_ptr<Mir::Instruction> &llvm_instruction : llvm_block->get_instructions()) {
            load_instruction(llvm_instruction, mir_block);
        }
    }
}

void Backend::MIR::Module::load_instruction(const std::shared_ptr<Mir::Instruction> &llvm_instruction, std::shared_ptr<Backend::MIR::Block> &mir_block) {
    switch (llvm_instruction->get_op()) {
            case Mir::Operator::LOAD: {
                std::shared_ptr<Mir::Load> load = std::dynamic_pointer_cast<Mir::Load>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Variable> load_from = find_variable(load->get_addr()->get_name(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Variable> load_to = std::make_shared<Backend::MIR::LocalVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*load->get_type()));
                mir_block->parent_function.lock()->add_variable(load_to);
                std::shared_ptr<Backend::MIR::MemoryInstruction> load_instruction = std::make_shared<Backend::MIR::MemoryInstruction>(Backend::MIR::InstructionType::LOAD, load_from, load_to);
                load_to->defined_in = load_instruction;
                load_from->used_in.push_back(load_instruction);
                mir_block->instructions.push_back(load_instruction);
                break;
            }
            case Mir::Operator::STORE: {
                std::shared_ptr<Mir::Store> store = std::dynamic_pointer_cast<Mir::Store>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Value> store_from = find_value(store->get_value(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Variable> store_to = find_variable(store->get_addr()->get_name(), mir_block->parent_function.lock());
                mir_block->instructions.push_back(std::make_shared<Backend::MIR::MemoryInstruction>(Backend::MIR::InstructionType::STORE, store_to, store_from));
                store_to->used_in.push_back(mir_block->instructions.back());
                if (store_from->value_type == Backend::MIR::OperandType::VARIABLE) {
                    std::shared_ptr<Backend::MIR::Variable> store_from_var = std::dynamic_pointer_cast<Backend::MIR::Variable>(store_from);
                    store_from_var->used_in.push_back(mir_block->instructions.back());
                }
                break;
            }
            case Mir::Operator::GEP: {
                std::shared_ptr<Mir::GetElementPtr> get_element_pointer = std::dynamic_pointer_cast<Mir::GetElementPtr>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Variable> array = find_variable(get_element_pointer->get_addr()->get_name(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Value> offset = find_value(get_element_pointer->get_index(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Variable> store_to = std::make_shared<Backend::MIR::LocalVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*get_element_pointer->get_type()));
                mir_block->parent_function.lock()->add_variable(store_to);
                store_to->defined_in = std::make_shared<Backend::MIR::MemoryInstruction>(Backend::MIR::InstructionType::LOAD_ADDR, array, offset);
                break;
            }
            // case Mir::Operator::BITCAST: {
            //     break;
            // }
            // case Mir::Operator::FPTOSI: {
            //     break;
            // }
            // case Mir::Operator::FCMP: {
            //     break;
            // }
            case Mir::Operator::ICMP: {
                std::shared_ptr<Mir::Icmp> icmp = std::dynamic_pointer_cast<Mir::Icmp>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Value> lhs = find_value(icmp->get_lhs(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Value> rhs = find_value(icmp->get_rhs(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Variable> result = std::make_shared<Backend::MIR::LocalVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*icmp->get_type()));
                mir_block->parent_function.lock()->add_variable(result);
                switch (icmp->op) {
                    case Mir::Icmp::Op::EQ: {
                        result->defined_in = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::BRANCH_ON_EQUAL, lhs, rhs, result);
                        break;
                    }
                    case Mir::Icmp::Op::NE: {
                        result->defined_in = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::BRANCH_ON_NOT_EQUAL, lhs, rhs, result);
                        break;
                    }
                    case Mir::Icmp::Op::GT: {
                        result->defined_in = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::BRANCH_ON_GREATER_THAN, lhs, rhs, result);
                        break;
                    }
                    case Mir::Icmp::Op::LT: {
                        result->defined_in = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::BRANCH_ON_LESS_THAN, lhs, rhs, result);
                        break;
                    }
                    case Mir::Icmp::Op::GE: {
                        result->defined_in = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::BRANCH_ON_GREATER_THAN_OR_EQUAL, lhs, rhs, result);
                        break;
                    }
                    case Mir::Icmp::Op::LE: {
                        result->defined_in = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::BRANCH_ON_LESS_THAN_OR_EQUAL, lhs, rhs, result);
                        break;
                    }
                    default: break;
                }
                break;
            }
            // case Mir::Operator::ZEXT: {
            //     break;
            // }
            case Mir::Operator::BRANCH: {
                std::shared_ptr<Mir::Branch> branch = std::dynamic_pointer_cast<Mir::Branch>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Block> block_true = mir_block->parent_function.lock()->blocks[branch->get_true_block()->get_name()];
                std::shared_ptr<Backend::MIR::Block> block_false = mir_block->parent_function.lock()->blocks[branch->get_false_block()->get_name()];
                std::shared_ptr<Backend::MIR::Variable> cond_value = find_variable(branch->get_cond()->get_name(), mir_block->parent_function.lock());
                block_true->predecessors.push_back(mir_block);
                block_false->predecessors.push_back(mir_block);
                mir_block->successors.push_back(block_true);
                mir_block->successors.push_back(block_false);
                mir_block->instructions.push_back(std::make_shared<Backend::MIR::BranchInstruction>(cond_value, block_true));
                mir_block->instructions.push_back(std::make_shared<Backend::MIR::BranchInstruction>(InstructionType::JUMP, block_false));
                break;
            }
            case Mir::Operator::JUMP: {
                std::shared_ptr<Mir::Jump> jump = std::dynamic_pointer_cast<Mir::Jump>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Block> target_block = mir_block->parent_function.lock()->blocks[jump->get_target_block()->get_name()];
                target_block->predecessors.push_back(mir_block);
                mir_block->successors.push_back(target_block);
                mir_block->instructions.push_back(std::make_shared<Backend::MIR::BranchInstruction>(InstructionType::JUMP, target_block));
                break;
            }
            case Mir::Operator::RET: {
                std::shared_ptr<Mir::Ret> ret = std::dynamic_pointer_cast<Mir::Ret>(llvm_instruction);
                if (!ret->get_type()->is_void()) {
                    std::shared_ptr<Backend::MIR::Value> return_value = find_value(ret->get_value(), mir_block->parent_function.lock());
                    mir_block->instructions.push_back(std::make_shared<Backend::MIR::ReturnInstruction>(return_value));
                    if (return_value->value_type == Backend::MIR::OperandType::VARIABLE) {
                        std::shared_ptr<Backend::MIR::Variable> return_var = std::dynamic_pointer_cast<Backend::MIR::Variable>(return_value);
                        return_var->used_in.push_back(mir_block->instructions.back());
                    }
                } else {
                    mir_block->instructions.push_back(std::make_shared<Backend::MIR::ReturnInstruction>());
                }
                break;
            }
            case Mir::Operator::CALL: {
                std::shared_ptr<Mir::Call> call = std::dynamic_pointer_cast<Mir::Call>(llvm_instruction);
                std::string function_name = call->get_function()->get_name();
                std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Value>>> function_params = std::make_shared<std::vector<std::shared_ptr<Backend::MIR::Value>>>();
                if (function_name == "putf") {
                    function_params->push_back(find_variable(".str_" + call->get_const_string_index(), mir_block->parent_function.lock()));
                    mir_block->instructions.push_back(std::make_shared<Backend::MIR::CallInstruction>(InstructionType::PUTF, function_params));
                    break;
                } else if (function_name.find("llvm.memset") == 0) {
                    break;
                } else {
                    for (std::shared_ptr<Mir::Value> param : call->get_params())
                        function_params->push_back(find_value(param, mir_block->parent_function.lock()));
                    mir_block->instructions.push_back(std::make_shared<Backend::MIR::CallInstruction>(this->functions[function_name], function_params));
                    if (!call->get_type()->is_void()) {
                        std::shared_ptr<Backend::MIR::Variable> store_to = std::make_shared<Backend::MIR::LocalVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*call->get_type()));
                        store_to->defined_in = mir_block->instructions.back();
                        mir_block->parent_function.lock()->add_variable(store_to);
                    }
                    for (std::shared_ptr<Backend::MIR::Value> param : *function_params)
                        if (param->value_type == Backend::MIR::OperandType::VARIABLE)
                            std::static_pointer_cast<Backend::MIR::Variable>(param)->used_in.push_back(mir_block->instructions.back());
                }
                break;
            }
            case Mir::Operator::INTBINARY: {
                std::shared_ptr<Mir::IntBinary> int_operation_ = std::dynamic_pointer_cast<Mir::IntBinary>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Value> lhs = find_value(int_operation_->get_lhs(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Value> rhs = find_value(int_operation_->get_rhs(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Variable> result = std::make_shared<Backend::MIR::LocalVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*int_operation_->get_type()));
                mir_block->parent_function.lock()->add_variable(result);
                std::shared_ptr<Backend::MIR::Instruction> mir_instruction;
                switch (int_operation_->op) {
                    case Mir::IntBinary::Op::ADD: {
                        mir_instruction = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::ADD, lhs, rhs, result);
                        break;
                    }
                    case Mir::IntBinary::Op::SUB: {
                        mir_instruction = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::SUB, lhs, rhs, result);
                        break;
                    }
                    case Mir::IntBinary::Op::MUL: {
                        mir_instruction = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::MUL, lhs, rhs, result);
                        break;
                    }
                    case Mir::IntBinary::Op::DIV: {
                        mir_instruction = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::DIV, lhs, rhs, result);
                        break;
                    }
                    case Mir::IntBinary::Op::MOD: {
                        mir_instruction = std::make_shared<Backend::MIR::ArithmeticInstruction>(InstructionType::MOD, lhs, rhs, result);
                        break;
                    }
                }
                result->defined_in = mir_instruction;
                if (lhs->value_type == Backend::MIR::OperandType::VARIABLE) {
                    std::shared_ptr<Backend::MIR::Variable> lhs_var = std::dynamic_pointer_cast<Backend::MIR::Variable>(lhs);
                    lhs_var->used_in.push_back(mir_instruction);
                }
                if (rhs->value_type == Backend::MIR::OperandType::VARIABLE) {
                    std::shared_ptr<Backend::MIR::Variable> rhs_var = std::dynamic_pointer_cast<Backend::MIR::Variable>(rhs);
                    rhs_var->used_in.push_back(mir_instruction);
                }
                mir_block->instructions.push_back(mir_instruction);
                break;
            }
            // case Mir::Operator::FLOATBINARY: {
            //     std::shared_ptr<Mir::FloatBinary> float_operation_ = std::dynamic_pointer_cast<Mir::FloatBinary>(instruction);
            //     switch (float_operation_->op) {
            //         case Mir::FloatBinary::Op::ADD: break;
            //         case Mir::FloatBinary::Op::SUB: break;
            //         case Mir::FloatBinary::Op::MUL: break;
            //         case Mir::FloatBinary::Op::DIV: break;
            //         case Mir::FloatBinary::Op::MOD: break;
            //     }
            //     break;
            // }
        default: break;
    }
}

void Backend::MIR::Module::load_functions_and_blocks(const std::shared_ptr<Mir::Module> &llvm_module) {
    for (const std::shared_ptr<Mir::Function> &llvm_function : llvm_module->get_functions()) {
        std::shared_ptr<Backend::MIR::Function> function = std::make_shared<Backend::MIR::Function>(llvm_function->get_name());
        function->return_type = Backend::Utils::llvm_to_riscv(*llvm_function->get_return_type());
        for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
            std::shared_ptr<Backend::MIR::Block> block = std::make_shared<Backend::MIR::Block>(llvm_block->get_name());
            block->parent_function = function;
            function->blocks[llvm_block->get_name()] = block;
        }
        functions[function->name] = function;
    }
}

void Backend::MIR::Module::load_global_data(const std::shared_ptr<Mir::Module> &llvm_module) {
    this->global_data = std::make_shared<Backend::DataSection>();
    this->global_data->load_global_variables(llvm_module->get_global_variables());
    this->global_data->load_global_variables(llvm_module->get_const_strings());
}
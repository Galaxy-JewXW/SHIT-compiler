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
        std::shared_ptr<Backend::MIR::Variable> var = std::make_shared<Backend::MIR::Parameter>(arg->get_name(), Backend::Utils::llvm_to_riscv(*arg->get_type()));
        mir_function->add_variable(var);
        mir_function->parameters.push_back(var);
    }
    for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
        std::shared_ptr<Backend::MIR::Block> mir_block = mir_function->blocks_index[llvm_block->get_name()];
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
                mir_block->instructions.push_back(std::make_shared<Backend::MIR::MemoryInstruction>(Backend::MIR::InstructionType::LOAD, load_from, load_to));
                break;
            }
            case Mir::Operator::STORE: {
                std::shared_ptr<Mir::Store> store = std::dynamic_pointer_cast<Mir::Store>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Value> store_from = find_value(store->get_value(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Variable> store_to = find_variable(store->get_addr()->get_name(), mir_block->parent_function.lock());
                mir_block->instructions.push_back(std::make_shared<Backend::MIR::MemoryInstruction>(Backend::MIR::InstructionType::STORE, store_to, store_from));
                break;
            }
            case Mir::Operator::GEP: {
                std::shared_ptr<Mir::GetElementPtr> get_element_pointer = std::dynamic_pointer_cast<Mir::GetElementPtr>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Variable> array = find_variable(get_element_pointer->get_addr()->get_name(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Value> offset = find_value(get_element_pointer->get_index(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Variable> store_to = std::make_shared<Backend::MIR::ElementPointer>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*get_element_pointer->get_type()), array, offset);
                mir_block->parent_function.lock()->add_variable(store_to);
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
                std::shared_ptr<Backend::MIR::Variable> result = std::make_shared<Backend::MIR::CompareVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*icmp->get_type()), lhs, rhs, Backend::Utils::llvm_to_mir(icmp->op));
                mir_block->parent_function.lock()->add_variable(result);
                break;
            }
            // case Mir::Operator::ZEXT: {
            //     break;
            // }
            case Mir::Operator::PHI: {
                std::shared_ptr<Mir::Phi> phi = std::dynamic_pointer_cast<Mir::Phi>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Variable> result = std::make_shared<Backend::MIR::LocalVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*phi->get_type()));
                mir_block->parent_function.lock()->add_variable(result);
                for (const std::pair<const std::shared_ptr<Mir::Block>, std::shared_ptr<Mir::Value>> &pair : phi->get_optional_values()) {
                    mir_block->parent_function.lock()->blocks_index[pair.first->get_name()]->instructions.push_back(std::make_shared<Backend::MIR::PhiInstruction>(find_value(pair.second, mir_block->parent_function.lock()), result));
                }
                break;
            }
            case Mir::Operator::BRANCH: {
                std::shared_ptr<Mir::Branch> branch = std::dynamic_pointer_cast<Mir::Branch>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Block> block_true = mir_block->parent_function.lock()->blocks_index[branch->get_true_block()->get_name()];
                std::shared_ptr<Backend::MIR::Block> block_false = mir_block->parent_function.lock()->blocks_index[branch->get_false_block()->get_name()];
                std::shared_ptr<Backend::MIR::CompareVariable> cond_value = std::static_pointer_cast<Backend::MIR::CompareVariable>(find_variable(branch->get_cond()->get_name(), mir_block->parent_function.lock()));
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
                std::shared_ptr<Backend::MIR::Block> target_block = mir_block->parent_function.lock()->blocks_index[jump->get_target_block()->get_name()];
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
                    if (!call->get_type()->is_void()) {
                        std::shared_ptr<Backend::MIR::Variable> store_to = std::make_shared<Backend::MIR::LocalVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*call->get_type()));
                        mir_block->parent_function.lock()->add_variable(store_to);
                        mir_block->instructions.push_back(std::make_shared<Backend::MIR::CallInstruction>(store_to, this->functions_index[function_name], function_params));
                    }
                }
                break;
            }
            case Mir::Operator::INTBINARY: {
                std::shared_ptr<Mir::IntBinary> int_operation_ = std::dynamic_pointer_cast<Mir::IntBinary>(llvm_instruction);
                std::shared_ptr<Backend::MIR::Value> lhs = find_value(int_operation_->get_lhs(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Value> rhs = find_value(int_operation_->get_rhs(), mir_block->parent_function.lock());
                std::shared_ptr<Backend::MIR::Variable> result = std::make_shared<Backend::MIR::LocalVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*int_operation_->get_type()));
                mir_block->parent_function.lock()->add_variable(result);
                mir_block->instructions.push_back(std::make_shared<Backend::MIR::ArithmeticInstruction>(Backend::Utils::llvm_to_mir(int_operation_->op), lhs, rhs, result));
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
    for (std::string function_name : Backend::MIR::privileged_function_names)
        add_function(std::make_shared<Backend::MIR::PrivilegedFunction>(function_name));
    for (const std::shared_ptr<Mir::Function> &llvm_function : llvm_module->get_functions()) {
        std::shared_ptr<Backend::MIR::Function> function = std::make_shared<Backend::MIR::Function>(llvm_function->get_name());
        function->return_type = Backend::Utils::llvm_to_riscv(*llvm_function->get_return_type());
        for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
            std::shared_ptr<Backend::MIR::Block> block = std::make_shared<Backend::MIR::Block>(llvm_block->get_name());
            block->parent_function = function;
            function->add_block(block);
        }
        add_function(function);
    }
}

void Backend::MIR::Module::load_global_data(const std::shared_ptr<Mir::Module> &llvm_module) {
    this->global_data = std::make_shared<Backend::DataSection>();
    this->global_data->load_global_variables(llvm_module->get_global_variables());
    this->global_data->load_global_variables(llvm_module->get_const_strings());
}

Backend::MIR::InstructionType Backend::Utils::llvm_to_mir(const Mir::IntBinary::Op &op) {
    switch (op) {
        case Mir::IntBinary::Op::ADD:
            return Backend::MIR::InstructionType::ADD;
        case Mir::IntBinary::Op::SUB:
            return Backend::MIR::InstructionType::SUB;
        case Mir::IntBinary::Op::MUL:
            return Backend::MIR::InstructionType::MUL;
        case Mir::IntBinary::Op::DIV:
            return Backend::MIR::InstructionType::DIV;
        default:
            return Backend::MIR::InstructionType::MOD;
    }
}

Backend::MIR::InstructionType Backend::Utils::llvm_to_mir(const Mir::Icmp::Op &op) {
    switch (op) {
        case Mir::Icmp::Op::EQ:
            return Backend::MIR::InstructionType::BRANCH_ON_EQUAL;
        case Mir::Icmp::Op::NE:
            return Backend::MIR::InstructionType::BRANCH_ON_NOT_EQUAL;
        case Mir::Icmp::Op::GT:
            return Backend::MIR::InstructionType::BRANCH_ON_GREATER_THAN;
        case Mir::Icmp::Op::LT:
            return Backend::MIR::InstructionType::BRANCH_ON_LESS_THAN;
        case Mir::Icmp::Op::GE:
            return Backend::MIR::InstructionType::BRANCH_ON_GREATER_THAN_OR_EQUAL;
        default:
            return Backend::MIR::InstructionType::BRANCH_ON_LESS_THAN_OR_EQUAL;
    }
}

void Backend::MIR::Module::analyze_live_variables() {
    for (std::shared_ptr<Backend::MIR::Function> function : functions) {
        if (dynamic_cast<Backend::MIR::PrivilegedFunction*>(function.get())) {
            continue;
        }
        function->analyze_live_variables();
    }
}

void Backend::MIR::Function::analyze_live_variables() {
    bool changed;
    do {
        changed = blocks.front()->analyze_live_variables(std::make_shared<std::vector<std::string>>());
    } while(changed);
    for (const std::shared_ptr<Backend::MIR::Block> &block : blocks) {
        for (const std::shared_ptr<Backend::MIR::Variable> &var : block->live_in) {
            if (var->position == Backend::MIR::VariablePosition::LOCAL) {
                var->position = Backend::MIR::VariablePosition::FUNCTION;
                log_debug("Variable %s in function %s is now a function variable", var->name.c_str(), name.c_str());
            }
        }
        for (const auto &var : block->live_out) {
            if (var->position == Backend::MIR::VariablePosition::LOCAL) {
                var->position = Backend::MIR::VariablePosition::FUNCTION;
                log_debug("Variable %s in function %s is now a function variable", var->name.c_str(), name.c_str());
            }
        }
    }
}

bool Backend::MIR::Block::analyze_live_variables(std::shared_ptr<std::vector<std::string>> visited) {
    bool changed = false;
    size_t old_in_size = live_in.size();
    size_t old_out_size = live_out.size();
    visited->push_back(name);
    // live_out = sum(live_in)
    for (const std::shared_ptr<Backend::MIR::Block> &succ : successors) {
        if (std::find(visited->begin(), visited->end(), succ->name) == visited->end()) {
            changed = changed || succ->analyze_live_variables(visited);
            live_out.insert(succ->live_in.begin(), succ->live_in.end());
        }
    }
    // live_in = (live_out - def) + use
    live_in.insert(live_out.begin(), live_out.end());
    for (std::vector<std::shared_ptr<Backend::MIR::Instruction>>::reverse_iterator it = instructions.rbegin(); it != instructions.rend(); ++it) {
        std::shared_ptr<Backend::MIR::Instruction> &instruction = *it;
        std::shared_ptr<Backend::MIR::Variable> def_var = instruction->get_defined_variable();
        if (def_var)
            live_in.erase(def_var);
        std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> used_vars = instruction->get_used_variables();
        for (const std::shared_ptr<Backend::MIR::Variable> &var : *used_vars)
            live_in.insert(var);
    }
    return changed || live_in.size() != old_in_size || live_out.size() != old_out_size;
}

void Backend::MIR::Module::print_live_variables() const {
    log_debug("====== Active Variables ======");
    for (const auto &function : functions) {
        if (dynamic_cast<Backend::MIR::PrivilegedFunction*>(function.get())) {
            continue;
        }
        log_debug("Function: %s", function->name.c_str());
        for (const auto &block : function->blocks) {
            log_debug("  Basic Block: %s", block->name.c_str());
            log_debug("    Live-in:");
            for (const auto &var : block->live_in) {
                log_debug(" %s", var->name.c_str());
            }
            log_debug("    Live-out:");
            for (const auto &var : block->live_out) {
                log_debug(" %s", var->name.c_str());
            }
        }
    }
    log_debug("============================");
}
#include "Backend/LIR/LIR.h"
#include "Backend/LIR/Instructions.h"

void Backend::LIR::Module::load_functional_variables(const std::shared_ptr<Mir::Function> &llvm_function, std::shared_ptr<Backend::LIR::Function> &mir_function) {
    size_t int_count = 0, float_count = 0;
    for (const std::shared_ptr<Mir::Argument> &llvm_arg : llvm_function->get_arguments()) {
        std::shared_ptr<Backend::Variable> arg;
        Backend::VariableType arg_type = Backend::Utils::llvm_to_riscv(*llvm_arg->get_type());
        std::shared_ptr<Backend::Variable> arg_ = std::make_shared<Backend::Variable>(llvm_arg->get_name(), arg_type, VariableWide::LOCAL);
        if (Backend::Utils::is_int(arg_type)) {
            if (int_count++ < 8) {
                arg = std::make_shared<Backend::Variable>(llvm_arg->get_name() + "_ireg", arg_type, VariableWide::LOCAL);
                mir_function->blocks.front()->instructions.push_back(std::make_shared<Move>(arg, arg_));
                mir_function->add_variable(arg_);
            } else {
                arg = std::make_shared<Backend::Pointer>(llvm_arg->get_name() + "_imem", Backend::Utils::to_pointer(arg_type), VariableWide::FUNCTIONAL);
                mir_function->blocks.front()->instructions.push_back(std::make_shared<LoadInt>(arg, arg_));
            }
        } else {
            if (float_count++ < 8) {
                arg = std::make_shared<Backend::Variable>(llvm_arg->get_name() + "_freg", arg_type, VariableWide::LOCAL);
                mir_function->blocks.front()->instructions.push_back(std::make_shared<Move>(arg, arg_));
                mir_function->add_variable(arg_);
            } else {
                arg = std::make_shared<Backend::Pointer>(llvm_arg->get_name() + "_fmem", Backend::Utils::to_pointer(arg_type), VariableWide::FUNCTIONAL);
                mir_function->blocks.front()->instructions.push_back(std::make_shared<LoadFloat>(arg, arg_));
            }
        }
        mir_function->add_variable(arg);
        mir_function->parameters.push_back(arg);
    }
    for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
        std::shared_ptr<Backend::LIR::Block> mir_block = mir_function->blocks_index[llvm_block->get_name()];
        for (const std::shared_ptr<Mir::Instruction> &llvm_instruction : llvm_block->get_instructions())
            if (llvm_instruction->get_op() == Mir::Operator::ALLOC) {
                std::shared_ptr<Mir::Alloc> alloc = std::static_pointer_cast<Mir::Alloc>(llvm_instruction);
                std::shared_ptr<Backend::Variable> var = std::make_shared<Backend::Variable>(alloc->get_name(), Backend::Utils::to_pointer(Backend::Utils::llvm_to_riscv(*alloc->get_type())), VariableWide::FUNCTIONAL);
                mir_function->add_variable(var);
            }
    }
}

void Backend::LIR::Module::load_functions_and_blocks() {
    for (std::shared_ptr<PrivilegedFunction> function : Backend::LIR::privileged_functions)
        add_function(function);
    for (const std::shared_ptr<Mir::Function> &llvm_function : llvm_module->get_functions()) {
        std::shared_ptr<Backend::LIR::Function> function = std::make_shared<Backend::LIR::Function>(llvm_function->get_name());
        function->return_type = Backend::Utils::llvm_to_riscv(*llvm_function->get_return_type());
        std::shared_ptr<Backend::LIR::Block> entry = std::make_shared<Backend::LIR::Block>("block_entry");
        entry->parent_function = function;
        function->add_block(entry);
        for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
            std::shared_ptr<Backend::LIR::Block> block = std::make_shared<Backend::LIR::Block>(llvm_block->get_name());
            block->parent_function = function;
            function->add_block(block);
        }
        add_function(function);
    }
}

void Backend::LIR::Module::load_instruction(const std::shared_ptr<Mir::Instruction> &llvm_instruction, std::shared_ptr<Backend::LIR::Block> &mir_block) {
    switch (llvm_instruction->get_op()) {
        case Mir::Operator::LOAD: {
            std::shared_ptr<Mir::Load> load = std::static_pointer_cast<Mir::Load>(llvm_instruction);
            std::shared_ptr<Backend::Variable> load_from = find_variable(load->get_addr()->get_name(), mir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> load_to = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*load->get_type()), VariableWide::LOCAL);
            mir_block->parent_function.lock()->add_variable(load_to);
            if (load_from->var_type != Variable::Type::PTR) {
                // global variable or allocated variable
                // global variables' address can use the same register of load_to, which will be overwritten.
                mir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadInt>(load_from, load_to));
            } else {
                // otherwise, load from an element pointer
                std::shared_ptr<Backend::Pointer> ep = std::static_pointer_cast<Backend::Pointer>(load_from);
                if (ep->offset->operand_type == Backend::OperandType::CONSTANT) {
                    mir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadInt>(ep->base, load_to, std::static_pointer_cast<Backend::IntValue>(ep->offset)->int32_value));
                } else {
                    // base & offset both variable, we need to calculate before load
                    std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR);
                    mir_block->parent_function.lock()->add_variable(base);
                    mir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(ep->base, base));
                    mir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, base, ep->offset, base));
                    mir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadInt>(base, load_to));
                }
            }
            break;
        }
        case Mir::Operator::STORE: {
            std::shared_ptr<Mir::Store> store = std::static_pointer_cast<Mir::Store>(llvm_instruction);
            std::shared_ptr<Backend::Variable> store_to = find_variable(store->get_addr()->get_name(), mir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> store_from = ensure_variable(find_operand(store->get_value(), mir_block->parent_function.lock()), mir_block);
            if (store_to->lifetime == VariableWide::GLOBAL) {
                std::shared_ptr<Backend::Variable> addr_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR);
                mir_block->parent_function.lock()->add_variable(addr_var);
                mir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(store_to, addr_var));
                mir_block->instructions.push_back(std::make_shared<Backend::LIR::StoreInt>(addr_var, store_from));
            } else if (store_to->var_type == Variable::Type::PTR) {
                std::shared_ptr<Backend::Pointer> ep = std::static_pointer_cast<Backend::Pointer>(store_to);
                std::shared_ptr<Backend::Variable> base = ep->base;
                if (base->lifetime == VariableWide::GLOBAL) {
                    base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
                    mir_block->parent_function.lock()->add_variable(base);
                    mir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(ep->base, base));
                }
                if (ep->offset->operand_type == Backend::OperandType::CONSTANT) {
                    mir_block->instructions.push_back(std::make_shared<Backend::LIR::StoreInt>(base, store_from, std::static_pointer_cast<Backend::IntValue>(ep->offset)->int32_value));
                } else {
                    base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
                    mir_block->parent_function.lock()->add_variable(base);
                    ep->base = base;
                    base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
                    mir_block->parent_function.lock()->add_variable(base);
                    mir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, ep->base, ep->offset, base));
                    mir_block->instructions.push_back(std::make_shared<Backend::LIR::StoreInt>(base, store_from));
                }
            } else {
                mir_block->instructions.push_back(std::make_shared<Backend::LIR::StoreInt>(store_to, store_from));
            }
            break;
        }
        case Mir::Operator::GEP: {
            std::shared_ptr<Mir::GetElementPtr> get_element_pointer = std::static_pointer_cast<Mir::GetElementPtr>(llvm_instruction);
            std::shared_ptr<Backend::Variable> pointer = find_variable(get_element_pointer->get_addr()->get_name(), mir_block->parent_function.lock());
            std::shared_ptr<Backend::Operand> offset = find_operand(get_element_pointer->get_index(), mir_block->parent_function.lock());
            mir_block->parent_function.lock()->add_variable(std::make_shared<Backend::Pointer>(llvm_instruction->get_name(), pointer, offset));
            break;
        }
        case Mir::Operator::FPTOSI: {
            break;
        }
        case Mir::Operator::FCMP: {
            break;
        }
        case Mir::Operator::ICMP: {
            std::shared_ptr<Mir::Icmp> icmp = std::static_pointer_cast<Mir::Icmp>(llvm_instruction);
            std::shared_ptr<Backend::Operand> lhs = find_operand(icmp->get_lhs(), mir_block->parent_function.lock());
            std::shared_ptr<Backend::Operand> rhs = find_operand(icmp->get_rhs(), mir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> lhs_var = ensure_variable(lhs, mir_block);
            std::shared_ptr<Backend::Variable> rhs_var = ensure_variable(rhs, mir_block);
            std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Comparison>(llvm_instruction->get_name(), lhs_var, rhs_var, Backend::Comparison::llvm_to_mir(icmp->op));
            mir_block->parent_function.lock()->add_variable(result);
            break;
        }
        case Mir::Operator::PHI: {
            // std::shared_ptr<Mir::Phi> phi = std::static_pointer_cast<Mir::Phi>(llvm_instruction);
            // std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::LIR::LocalVariable>(
            //     llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*phi->get_type()));
            // mir_block->parent_function.lock()->add_variable(result);
            // std::shared_ptr<Backend::LIR::PhiInstruction> phi_inst = std::make_shared<Backend::LIR::PhiInstruction>(result);
            // for (const std::pair<const std::shared_ptr<Mir::Block>, std::shared_ptr<Mir::Value>> &pair : phi->get_optional_values()) {
            //     std::shared_ptr<Backend::LIR::Value> value = find_operand(pair.second, mir_block->parent_function.lock());
            //     std::shared_ptr<Backend::LIR::Block> block = mir_block->parent_function.lock()->blocks_index[pair.first->get_name()];
            //     phi_inst->add_phi_value(value, block);
            // }
            // mir_block->instructions.push_back(phi_inst);
            break;
        }
        case Mir::Operator::BRANCH: {
            // TODO
            std::shared_ptr<Mir::Branch> branch = std::static_pointer_cast<Mir::Branch>(llvm_instruction);
            std::shared_ptr<Backend::LIR::Block> block_true = mir_block->parent_function.lock()->blocks_index[branch->get_true_block()->get_name()];
            std::shared_ptr<Backend::LIR::Block> block_false = mir_block->parent_function.lock()->blocks_index[branch->get_false_block()->get_name()];
            std::shared_ptr<Backend::Comparison> cond_var = std::static_pointer_cast<Backend::Comparison>(find_variable(branch->get_cond()->get_name(), mir_block->parent_function.lock()));
            block_true->predecessors.push_back(mir_block);
            block_false->predecessors.push_back(mir_block);
            mir_block->successors.push_back(block_true);
            mir_block->successors.push_back(block_false);
            mir_block->instructions.push_back(std::make_shared<Backend::LIR::BranchInstruction>(cond_var->compare_type, cond_var->lhs, cond_var->rhs, block_true));
            mir_block->instructions.push_back(std::make_shared<Backend::LIR::JumpInstruction>(block_false));
            break;
        }
        case Mir::Operator::JUMP: {
            std::shared_ptr<Mir::Jump> jump = std::static_pointer_cast<Mir::Jump>(llvm_instruction);
            std::shared_ptr<Backend::LIR::Block> target_block = mir_block->parent_function.lock()->blocks_index[jump->get_target_block()->get_name()];
            target_block->predecessors.push_back(mir_block);
            mir_block->successors.push_back(target_block);
            mir_block->instructions.push_back(std::make_shared<Backend::LIR::JumpInstruction>(target_block));
            break;
        }
        case Mir::Operator::RET: {
            std::shared_ptr<Mir::Ret> ret = std::static_pointer_cast<Mir::Ret>(llvm_instruction);
            if (!ret->get_type()->is_void())
                mir_block->instructions.push_back(std::make_shared<Backend::LIR::ReturnInstruction>(ensure_variable(find_operand(ret->get_value(), mir_block->parent_function.lock()), mir_block)));
            else
                mir_block->instructions.push_back(std::make_shared<Backend::LIR::ReturnInstruction>());
            break;
        }
        case Mir::Operator::CALL: {
            // std::shared_ptr<Mir::Call> call = std::static_pointer_cast<Mir::Call>(llvm_instruction);
            // std::string function_name = call->get_function()->get_name();
            // std::shared_ptr<std::vector<std::shared_ptr<Backend::LIR::Value>>> function_params = std::make_shared<std::vector<std::shared_ptr<Backend::LIR::Value>>>();
            // if (function_name == "putf") {
            //     function_params->push_back(find_variable(".str_" + call->get_const_string_index(), mir_block->parent_function.lock()));
            //     mir_block->instructions.push_back(std::make_shared<Backend::LIR::Call>(InstructionType::PUTF, function_params));
            //     break;
            // } else if (function_name.find("llvm.memset") == 0) {
            //     break;
            // } else {
            //     for (std::shared_ptr<Mir::Value> param : call->get_params())
            //         function_params->push_back(find_operand(param, mir_block->parent_function.lock()));
            //     if (!call->get_type()->is_void()) {
            //         std::shared_ptr<Backend::Variable> store_to = std::make_shared<Backend::LIR::LocalVariable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*call->get_type()));
            //         mir_block->parent_function.lock()->add_variable(store_to);
            //         mir_block->instructions.push_back(std::make_shared<Backend::LIR::Call>(store_to, this->functions_index[function_name], function_params));
            //     }
            // }
            break;
        }
        case Mir::Operator::INTBINARY: {
            std::shared_ptr<Mir::IntBinary> int_operation_ = std::static_pointer_cast<Mir::IntBinary>(llvm_instruction);
            std::shared_ptr<Backend::Operand> lhs = find_operand(int_operation_->get_lhs(), mir_block->parent_function.lock());
            std::shared_ptr<Backend::Operand> rhs = find_operand(int_operation_->get_rhs(), mir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*int_operation_->get_type()), VariableWide::LOCAL);
            mir_block->parent_function.lock()->add_variable(result);
            if (lhs->operand_type == Backend::OperandType::CONSTANT && rhs->operand_type == Backend::OperandType::CONSTANT) {
                int32_t result_value = Backend::Utils::compute<int32_t>(Backend::Utils::llvm_to_mir(int_operation_->op), std::static_pointer_cast<Backend::IntValue>(lhs)->int32_value, std::static_pointer_cast<Backend::IntValue>(rhs)->int32_value);
                mir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadI32>(std::make_shared<Backend::IntValue>(result_value), result));
                break;
            } else if (lhs->operand_type == Backend::OperandType::CONSTANT) {
                std::swap(lhs, rhs);
            }
            mir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::Utils::llvm_to_mir(int_operation_->op), std::static_pointer_cast<Backend::Variable>(lhs), rhs, result));
            break;
        }
        case Mir::Operator::FLOATBINARY: {
            std::shared_ptr<Mir::FloatBinary> float_operation_ = std::static_pointer_cast<Mir::FloatBinary>(llvm_instruction);
            std::shared_ptr<Backend::Operand> lhs = find_operand(float_operation_->get_lhs(), mir_block->parent_function.lock());
            std::shared_ptr<Backend::Operand> rhs = find_operand(float_operation_->get_rhs(), mir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> lhs_var = ensure_variable(lhs, mir_block);
            std::shared_ptr<Backend::Variable> rhs_var = ensure_variable(rhs, mir_block);
            std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*float_operation_->get_type()), VariableWide::LOCAL);
            mir_block->parent_function.lock()->add_variable(result);
            mir_block->instructions.push_back(std::make_shared<Backend::LIR::FloatArithmetic>(Backend::Utils::llvm_to_mir(float_operation_->op), lhs_var, rhs_var, result));
            break;
        }
        default: break;
    }
}
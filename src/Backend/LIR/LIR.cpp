#include "Backend/LIR/LIR.h"
#include "Backend/LIR/Instructions.h"

std::shared_ptr<Backend::Variable> Backend::LIR::Module::ensure_variable(const std::shared_ptr<Backend::Operand> &value, std::shared_ptr<Backend::LIR::Block> &block) {
    if (value->operand_type == OperandType::CONSTANT) {
        std::shared_ptr<Backend::Constant> constant = std::static_pointer_cast<Backend::Constant>(value);
        std::shared_ptr<Backend::Variable> temp_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("temp_constant"), constant->constant_type, VariableWide::LOCAL);
        block->parent_function.lock()->add_variable(temp_var);
        block->instructions.push_back(std::make_shared<Backend::LIR::LoadI32>(temp_var, constant));
        return temp_var;
    }
    return std::static_pointer_cast<Backend::Variable>(value);
}

void Backend::LIR::Module::load_functional_variables(const std::shared_ptr<Mir::Function> &llvm_function, std::shared_ptr<Backend::LIR::Function> &lir_function) {
    size_t int_count = 0, float_count = 0;
    for (const std::shared_ptr<Mir::Argument> &llvm_arg : llvm_function->get_arguments()) {
        std::shared_ptr<Backend::Variable> arg;
        Backend::VariableType arg_type = Backend::Utils::llvm_to_riscv(*llvm_arg->get_type());
        std::shared_ptr<Backend::Variable> arg_ = std::make_shared<Backend::Variable>(llvm_arg->get_name(), arg_type, VariableWide::LOCAL);
        if (Backend::Utils::is_int(arg_type)) {
            if (int_count++ < 8) {
                arg = std::make_shared<Backend::Variable>(llvm_arg->get_name() + "_ireg", arg_type, VariableWide::LOCAL);
                lir_function->blocks.front()->instructions.push_back(std::make_shared<Move>(arg, arg_));
                lir_function->add_variable(arg_);
            } else {
                arg = std::make_shared<Backend::Variable>(llvm_arg->get_name() + "_imem", Backend::Utils::to_pointer(arg_type), VariableWide::FUNCTIONAL);
                lir_function->blocks.front()->instructions.push_back(std::make_shared<LoadInt>(arg, arg_));
            }
        } else {
            if (float_count++ < 8) {
                arg = std::make_shared<Backend::Variable>(llvm_arg->get_name() + "_freg", arg_type, VariableWide::LOCAL);
                lir_function->blocks.front()->instructions.push_back(std::make_shared<Move>(arg, arg_));
                lir_function->add_variable(arg_);
            } else {
                arg = std::make_shared<Backend::Variable>(llvm_arg->get_name() + "_fmem", Backend::Utils::to_pointer(arg_type), VariableWide::FUNCTIONAL);
                lir_function->blocks.front()->instructions.push_back(std::make_shared<LoadFloat>(arg, arg_));
            }
        }
        lir_function->add_variable(arg);
        lir_function->parameters.push_back(arg);
    }
    for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
        std::shared_ptr<Backend::LIR::Block> mir_block = lir_function->blocks_index[llvm_block->get_name()];
        for (const std::shared_ptr<Mir::Instruction> &llvm_instruction : llvm_block->get_instructions())
            if (llvm_instruction->get_op() == Mir::Operator::ALLOC) {
                std::shared_ptr<Mir::Alloc> alloc = std::static_pointer_cast<Mir::Alloc>(llvm_instruction);
                std::shared_ptr<Backend::Variable> var = std::make_shared<Backend::Variable>(alloc->get_name(), Backend::Utils::to_pointer(Backend::Utils::llvm_to_riscv(*alloc->get_type())), VariableWide::FUNCTIONAL);
                lir_function->add_variable(var);
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
                    std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
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
                std::shared_ptr<Backend::Variable> addr_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
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
            if (lhs->operand_type == OperandType::VARIABLE)
                mir_block->parent_function.lock()->add_variable(std::make_shared<Backend::Comparison>(llvm_instruction->get_name(), std::static_pointer_cast<Backend::Variable>(lhs), rhs, Backend::Comparison::load_from_llvm(icmp->op)));
            else if (rhs->operand_type == OperandType::VARIABLE)
                mir_block->parent_function.lock()->add_variable(std::make_shared<Backend::Comparison>(llvm_instruction->get_name(), lhs, std::static_pointer_cast<Backend::Variable>(rhs), Backend::Comparison::load_from_llvm(icmp->op)));
            else
                log_error("We shall not compare 2 certain values in backend!");
            break;
        }
        case Mir::Operator::PHI: {
            // std::shared_ptr<Mir::Phi> phi = std::static_pointer_cast<Mir::Phi>(llvm_instruction);
            // TODO
            break;
        }
        case Mir::Operator::BRANCH: {
            std::shared_ptr<Mir::Branch> branch = std::static_pointer_cast<Mir::Branch>(llvm_instruction);
            std::shared_ptr<Backend::LIR::Block> block_true = mir_block->parent_function.lock()->blocks_index[branch->get_true_block()->get_name()];
            std::shared_ptr<Backend::LIR::Block> block_false = mir_block->parent_function.lock()->blocks_index[branch->get_false_block()->get_name()];
            std::shared_ptr<Backend::Comparison> cond_var = std::static_pointer_cast<Backend::Comparison>(find_variable(branch->get_cond()->get_name(), mir_block->parent_function.lock()));
            block_true->predecessors.push_back(mir_block);
            block_false->predecessors.push_back(mir_block);
            mir_block->successors.push_back(block_true);
            mir_block->successors.push_back(block_false);
            if (cond_var->rhs->operand_type == OperandType::CONSTANT) {
                std::shared_ptr<Backend::Constant> rhs = std::static_pointer_cast<Backend::Constant>(cond_var->rhs);
                if (rhs->constant_type == VariableType::INT32) {
                    if (std::static_pointer_cast<Backend::IntValue>(rhs)->int32_value == 0) {
                        mir_block->instructions.push_back(std::make_shared<Backend::LIR::BranchInstruction>(Backend::Utils::cmp_to_lir_zero(cond_var->compare_type), cond_var->lhs, std::static_pointer_cast<Backend::Variable>(cond_var->rhs), block_true));
                        break;
                    }
                }
            }
            std::shared_ptr<Backend::Variable> rhs = ensure_variable(cond_var->rhs, mir_block);
            mir_block->instructions.push_back(std::make_shared<Backend::LIR::BranchInstruction>(Backend::Utils::cmp_to_lir(cond_var->compare_type), cond_var->lhs, rhs, block_true));
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
                int32_t result_value = Backend::Utils::compute<int32_t>(Backend::Utils::llvm_to_lir(int_operation_->op), std::static_pointer_cast<Backend::IntValue>(lhs)->int32_value, std::static_pointer_cast<Backend::IntValue>(rhs)->int32_value);
                mir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadI32>(result, std::make_shared<Backend::IntValue>(result_value)));
                break;
            } else if (lhs->operand_type == Backend::OperandType::CONSTANT) {
                std::swap(lhs, rhs);
            }
            mir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::Utils::llvm_to_lir(int_operation_->op), std::static_pointer_cast<Backend::Variable>(lhs), rhs, result));
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
            mir_block->instructions.push_back(std::make_shared<Backend::LIR::FloatArithmetic>(Backend::Utils::llvm_to_lir(float_operation_->op), lhs_var, rhs_var, result));
            break;
        }
        default: break;
    }
}
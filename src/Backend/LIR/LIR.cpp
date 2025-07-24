#include "Backend/LIR/LIR.h"
#include "Backend/LIR/Instructions.h"

std::shared_ptr<Backend::Variable> Backend::LIR::Module::ensure_variable(const std::shared_ptr<Backend::Operand> &value, std::shared_ptr<Backend::LIR::Block> &block) {
    if (value->operand_type == OperandType::CONSTANT) {
        std::shared_ptr<Backend::Constant> constant = std::static_pointer_cast<Backend::Constant>(value);
        std::shared_ptr<Backend::Variable> temp_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("temp_constant"), constant->constant_type, VariableWide::LOCAL);
        block->parent_function.lock()->add_variable(temp_var);
        if (constant->constant_type == Backend::VariableType::INT32)
            block->instructions.push_back(std::make_shared<Backend::LIR::LoadIntImm>(temp_var, std::static_pointer_cast<Backend::IntValue>(constant)));
        else
            block->instructions.push_back(std::make_shared<Backend::LIR::LoadFloatImm>(temp_var, std::static_pointer_cast<Backend::FloatValue>(constant)));
        return temp_var;
    }
    return std::static_pointer_cast<Backend::Variable>(value);
}

void Backend::LIR::Module::load_functional_variables(const std::shared_ptr<Mir::Function> &llvm_function, std::shared_ptr<Backend::LIR::Function> &lir_function) {
    for (const std::shared_ptr<Mir::Argument> &llvm_arg : llvm_function->get_arguments()) {
        Backend::VariableType arg_type = Backend::Utils::llvm_to_riscv(*llvm_arg->get_type());
        std::shared_ptr<Backend::Variable> arg = std::make_shared<Backend::Variable>(llvm_arg->get_name(), arg_type, VariableWide::LOCAL);
        lir_function->add_variable(arg);
        lir_function->parameters.push_back(arg);
    }
    for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
        std::shared_ptr<Backend::LIR::Block> lir_block = lir_function->blocks_index[llvm_block->get_name()];
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
        for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
            std::shared_ptr<Backend::LIR::Block> block = std::make_shared<Backend::LIR::Block>(llvm_block->get_name());
            block->parent_function = function;
            function->add_block(block);
        }
        add_function(function);
    }
}

void Backend::LIR::Module::load_instruction(const std::shared_ptr<Mir::Instruction> &llvm_instruction, std::shared_ptr<Backend::LIR::Block> &lir_block) {
    switch (llvm_instruction->get_op()) {
        case Mir::Operator::MOVE: {
            std::shared_ptr<Mir::Move> move = std::static_pointer_cast<Mir::Move>(llvm_instruction);
            std::shared_ptr<Backend::Variable> move_from = ensure_variable(find_operand(move->get_from_value(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> move_to = std::make_shared<Backend::Variable>(move->get_to_value()->get_name(), Backend::Utils::llvm_to_riscv(*move->get_to_value()->get_type()), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(move_to);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::Move>(move_from, move_to));
            break;
        }
        case Mir::Operator::LOAD: {
            std::shared_ptr<Mir::Load> load = std::static_pointer_cast<Mir::Load>(llvm_instruction);
            std::shared_ptr<Backend::Variable> load_from = find_variable(load->get_addr()->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> load_to = std::make_shared<Backend::Variable>(load->get_name(), Backend::Utils::llvm_to_riscv(*load->get_type()), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(load_to);
            if (load_from->var_type != Variable::Type::PTR) {
                // global variable or allocated variable
                // global variables' address can use the same register of load_to, which will be overwritten.
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadInt>(load_from, load_to));
            } else {
                // otherwise, load from an element pointer
                std::shared_ptr<Backend::Pointer> ep = std::static_pointer_cast<Backend::Pointer>(load_from);
                if (ep->offset->operand_type == Backend::OperandType::CONSTANT) {
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadInt>(ep->base, load_to, std::static_pointer_cast<Backend::IntValue>(ep->offset)->int32_value * Backend::Utils::type_to_size(ep->base->workload_type)));
                } else {
                    // base & offset both variable, we need to calculate before load
                    std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
                    lir_block->parent_function.lock()->add_variable(base);
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(ep->base, base));
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, base, ep->offset, base));
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadInt>(base, load_to));
                }
            }
            break;
        }
        case Mir::Operator::STORE: {
            std::shared_ptr<Mir::Store> store = std::static_pointer_cast<Mir::Store>(llvm_instruction);
            std::shared_ptr<Backend::Variable> store_to = find_variable(store->get_addr()->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> store_from = ensure_variable(find_operand(store->get_value(), lir_block->parent_function.lock()), lir_block);
            if (store_to->lifetime == VariableWide::GLOBAL) {
                std::shared_ptr<Backend::Variable> addr_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
                lir_block->parent_function.lock()->add_variable(addr_var);
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(store_to, addr_var));
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::StoreInt>(addr_var, store_from));
            } else if (store_to->var_type == Variable::Type::PTR) {
                std::shared_ptr<Backend::Pointer> ep = std::static_pointer_cast<Backend::Pointer>(store_to);
                std::shared_ptr<Backend::Variable> base = ep->base;
                if (base->lifetime == VariableWide::GLOBAL) {
                    base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
                    lir_block->parent_function.lock()->add_variable(base);
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(ep->base, base));
                }
                if (ep->offset->operand_type == Backend::OperandType::CONSTANT) {
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::StoreInt>(base, store_from, std::static_pointer_cast<Backend::IntValue>(ep->offset)->int32_value * Backend::Utils::type_to_size(ep->base->workload_type)));
                } else {
                    base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
                    lir_block->parent_function.lock()->add_variable(base);
                    ep->base = base;
                    base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::VariableType::INT32_PTR, VariableWide::LOCAL);
                    lir_block->parent_function.lock()->add_variable(base);
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, ep->base, ep->offset, base));
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::StoreInt>(base, store_from));
                }
            } else {
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::StoreInt>(store_to, store_from));
            }
            break;
        }
        case Mir::Operator::GEP: {
            std::shared_ptr<Mir::GetElementPtr> get_element_pointer = std::static_pointer_cast<Mir::GetElementPtr>(llvm_instruction);
            std::shared_ptr<Backend::Variable> pointer = find_variable(get_element_pointer->get_addr()->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Operand> offset = find_operand(get_element_pointer->get_index(), lir_block->parent_function.lock());
            lir_block->parent_function.lock()->add_variable(std::make_shared<Backend::Pointer>(llvm_instruction->get_name(), pointer, offset));
            break;
        }
        case Mir::Operator::FPTOSI: {
            // TODO: check up
            std::shared_ptr<Mir::Fptosi> fptosi = std::static_pointer_cast<Mir::Fptosi>(llvm_instruction);
            std::shared_ptr<Backend::Variable> source = find_variable(fptosi->get_operands()[1]->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> dest = find_variable(fptosi->get_operands()[0]->get_name(), lir_block->parent_function.lock());
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::Convert>(Backend::LIR::InstructionType::F2I, source, dest));
            break;
        }
        case Mir::Operator::SITOFP: {
            // TODO: check up
            std::shared_ptr<Mir::Sitofp> sitofp = std::static_pointer_cast<Mir::Sitofp>(llvm_instruction);
            std::shared_ptr<Backend::Variable> source = find_variable(sitofp->get_operands()[1]->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> dest = find_variable(sitofp->get_operands()[0]->get_name(), lir_block->parent_function.lock());
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::Convert>(Backend::LIR::InstructionType::I2F, source, dest));
            break;
        }
        case Mir::Operator::FCMP: {
            // std::shared_ptr<Mir::Fcmp> icmp = std::static_pointer_cast<Mir::Fcmp>(llvm_instruction);
            // std::shared_ptr<Backend::Variable> lhs = find_variable(icmp->get_lhs()->get_name(), lir_block->parent_function.lock());
            // std::shared_ptr<Backend::Variable> rhs = find_variable(icmp->get_rhs()->get_name(), lir_block->parent_function.lock());
            // std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("comparison"))
            // if (lhs->operand_type == OperandType::VARIABLE)
            //     lir_block->parent_function.lock()->add_variable(std::make_shared<Backend::Comparison>(llvm_instruction->get_name(), std::static_pointer_cast<Backend::Variable>(lhs), rhs, Backend::Comparison::load_from_llvm(icmp->op)));
            // else if (rhs->operand_type == OperandType::VARIABLE)
            //     lir_block->parent_function.lock()->add_variable(std::make_shared<Backend::Comparison>(llvm_instruction->get_name(), lhs, std::static_pointer_cast<Backend::Variable>(rhs), Backend::Comparison::load_from_llvm(icmp->op)));
            // else
            //     log_error("We shall not compare 2 certain values in backend!");
            break;
        }
        case Mir::Operator::ICMP: {
            std::shared_ptr<Mir::Icmp> icmp = std::static_pointer_cast<Mir::Icmp>(llvm_instruction);
            std::shared_ptr<Backend::Operand> lhs = find_operand(icmp->get_lhs(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Operand> rhs = find_operand(icmp->get_rhs(), lir_block->parent_function.lock());
            if (lhs->operand_type == OperandType::VARIABLE)
                lir_block->parent_function.lock()->add_variable(std::make_shared<Backend::Comparison>(llvm_instruction->get_name(), std::static_pointer_cast<Backend::Variable>(lhs), rhs, Backend::Comparison::load_from_llvm(icmp->op)));
            else if (rhs->operand_type == OperandType::VARIABLE)
                lir_block->parent_function.lock()->add_variable(std::make_shared<Backend::Comparison>(llvm_instruction->get_name(), lhs, std::static_pointer_cast<Backend::Variable>(rhs), Backend::Comparison::load_from_llvm(icmp->op)));
            else
                log_error("We shall not compare 2 certain values in backend!");
            break;
        }
        case Mir::Operator::BRANCH: {
            std::shared_ptr<Mir::Branch> branch = std::static_pointer_cast<Mir::Branch>(llvm_instruction);
            std::shared_ptr<Backend::LIR::Block> block_true = lir_block->parent_function.lock()->blocks_index[branch->get_true_block()->get_name()];
            std::shared_ptr<Backend::LIR::Block> block_false = lir_block->parent_function.lock()->blocks_index[branch->get_false_block()->get_name()];
            std::shared_ptr<Backend::Comparison> cond_var = std::static_pointer_cast<Backend::Comparison>(find_variable(branch->get_cond()->get_name(), lir_block->parent_function.lock()));
            block_true->predecessors.push_back(lir_block);
            block_false->predecessors.push_back(lir_block);
            lir_block->successors.push_back(block_true);
            lir_block->successors.push_back(block_false);
            if (cond_var->rhs->operand_type == OperandType::CONSTANT) {
                std::shared_ptr<Backend::Constant> rhs = std::static_pointer_cast<Backend::Constant>(cond_var->rhs);
                if (rhs->constant_type == VariableType::INT32) {
                    if (std::static_pointer_cast<Backend::IntValue>(rhs)->int32_value == 0) {
                        lir_block->instructions.push_back(std::make_shared<Backend::LIR::BranchInstruction>(Backend::Utils::cmp_to_lir_zero(cond_var->compare_type), cond_var->lhs, block_true));
                        break;
                    }
                }
            }
            std::shared_ptr<Backend::Variable> rhs = ensure_variable(cond_var->rhs, lir_block);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::BranchInstruction>(Backend::Utils::cmp_to_lir(cond_var->compare_type), cond_var->lhs, rhs, block_true));
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::Jump>(block_false));
            break;
        }
        case Mir::Operator::JUMP: {
            std::shared_ptr<Mir::Jump> jump = std::static_pointer_cast<Mir::Jump>(llvm_instruction);
            std::shared_ptr<Backend::LIR::Block> target_block = lir_block->parent_function.lock()->blocks_index[jump->get_target_block()->get_name()];
            target_block->predecessors.push_back(lir_block);
            lir_block->successors.push_back(target_block);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::Jump>(target_block));
            break;
        }
        case Mir::Operator::RET: {
            std::shared_ptr<Mir::Ret> ret = std::static_pointer_cast<Mir::Ret>(llvm_instruction);
            if (ret->get_type())
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::Return>(ensure_variable(find_operand(ret->get_value(), lir_block->parent_function.lock()), lir_block)));
            else
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::Return>());
            break;
        }
        case Mir::Operator::CALL: {
            // TODO: handle `putf` separately
            lir_block->parent_function.lock()->is_caller = true;
            std::shared_ptr<Mir::Call> call = std::static_pointer_cast<Mir::Call>(llvm_instruction);
            std::string function_name = call->get_function()->get_name();
            std::vector<std::shared_ptr<Backend::Variable>> function_params;
            if (function_name.find("llvm.memset") != 0) {
                for (std::shared_ptr<Mir::Value> param : call->get_params())
                    function_params.push_back(ensure_variable(find_operand(param, lir_block->parent_function.lock()), lir_block));
                if (!call->get_type()->is_void()) {
                    std::shared_ptr<Backend::Variable> store_to = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*call->get_type()), VariableWide::LOCAL);
                    lir_block->parent_function.lock()->add_variable(store_to);
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::Call>(store_to, functions_index[function_name], function_params));
                } else {
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::Call>(functions_index[function_name], function_params));
                }
            }
            break;
        }
        case Mir::Operator::INTBINARY: {
            std::shared_ptr<Mir::IntBinary> int_operation_ = std::static_pointer_cast<Mir::IntBinary>(llvm_instruction);
            std::shared_ptr<Backend::Operand> lhs = find_operand(int_operation_->get_lhs(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Operand> rhs = find_operand(int_operation_->get_rhs(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*int_operation_->get_type()), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(result);
            if (lhs->operand_type == Backend::OperandType::CONSTANT && rhs->operand_type == Backend::OperandType::CONSTANT) {
                int32_t result_value = Backend::Utils::compute<int32_t>(Backend::Utils::llvm_to_lir(int_operation_->op), std::static_pointer_cast<Backend::IntValue>(lhs)->int32_value, std::static_pointer_cast<Backend::IntValue>(rhs)->int32_value);
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadIntImm>(result, std::make_shared<Backend::IntValue>(result_value)));
                break;
            } else if (lhs->operand_type == Backend::OperandType::CONSTANT) {
                std::swap(lhs, rhs);
            }
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::Utils::llvm_to_lir(int_operation_->op), std::static_pointer_cast<Backend::Variable>(lhs), rhs, result));
            break;
        }
        case Mir::Operator::FLOATBINARY: {
            std::shared_ptr<Mir::FloatBinary> float_operation_ = std::static_pointer_cast<Mir::FloatBinary>(llvm_instruction);
            std::shared_ptr<Backend::Operand> lhs = find_operand(float_operation_->get_lhs(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Operand> rhs = find_operand(float_operation_->get_rhs(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> lhs_var = ensure_variable(lhs, lir_block);
            std::shared_ptr<Backend::Variable> rhs_var = ensure_variable(rhs, lir_block);
            std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*float_operation_->get_type()), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(result);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::FloatArithmetic>(Backend::Utils::llvm_to_lir(float_operation_->op), lhs_var, rhs_var, result));
            break;
        }
        default: break;
    }
}

void Backend::LIR::Function::spill(std::shared_ptr<Backend::Variable> &local_variable) {
    if (local_variable->lifetime != VariableWide::LOCAL)
        log_error("Only variable in register can be spilled.");
    local_variable->lifetime = VariableWide::FUNCTIONAL;
    for (std::shared_ptr<Backend::LIR::Block> &block : blocks) {
        for (size_t i = 0; i < block->instructions.size(); i++) {
            std::shared_ptr<Backend::LIR::Instruction> &instr = block->instructions[i];
            if (instr->get_defined_variable() == local_variable) {
                // insert `store` after the instruction
                std::shared_ptr<Backend::Variable> new_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("spill_"), local_variable->workload_type, VariableWide::LOCAL);
                instr->update_defined_variable(new_var);
                log_debug("Spilling variable %s to %s", local_variable->name.c_str(), new_var->name.c_str());
                block->instructions.insert(block->instructions.begin() + i + 1, std::make_shared<Backend::LIR::StoreInt>(local_variable, new_var));
            } else if (std::find(instr->get_used_variables().begin(), instr->get_used_variables().end(), local_variable) != instr->get_used_variables().end()) {
                // insert `load` before the instruction
                std::shared_ptr<Backend::Variable> new_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("spill_"), local_variable->workload_type, VariableWide::LOCAL);
                instr->update_used_variable(local_variable, new_var);
                log_debug("Loading spilled variable %s to %s", local_variable->name.c_str(), new_var->name.c_str());
                block->instructions.insert(block->instructions.begin() + i, std::make_shared<Backend::LIR::LoadInt>(new_var, local_variable));
            }
        }
    }
}

void Backend::LIR::Function::analyze_live_variables() {
    bool changed = true;
    for (std::shared_ptr<Backend::LIR::Block> &block : blocks) {
        block->live_in.clear();
        block->live_out.clear();
    }
    while(changed) {
        std::unordered_set<std::string> visited;
        std::shared_ptr<Backend::LIR::Block> first_block = blocks.front();
        changed = analyze_live_variables(first_block, visited);
    }
    // std::cout << live_variables();
}

bool Backend::LIR::Function::analyze_live_variables(std::shared_ptr<Backend::LIR::Block> &block, std::unordered_set<std::string> &visited) {
    bool changed = false;
    size_t old_in_size = block->live_in.size();
    size_t old_out_size = block->live_out.size();
    visited.insert(block->name);
    // live_out = sum(live_in)
    for (std::shared_ptr<Backend::LIR::Block> &succ : block->successors) {
        if (visited.find(succ->name) == visited.end())
            changed = analyze_live_variables(succ, visited) | changed;
        block->live_out.insert(succ->live_in.begin(), succ->live_in.end());
    }
    // live_in = (live_out - def) + use
    block->live_in.insert(block->live_out.begin(), block->live_out.end());
    for (std::vector<std::shared_ptr<Backend::LIR::Instruction>>::reverse_iterator it = block->instructions.rbegin(); it != block->instructions.rend(); it++) {
        std::shared_ptr<Backend::LIR::Instruction> &instruction = *it;
        std::shared_ptr<Backend::Variable> def_var = instruction->get_defined_variable();
        if (def_var)
            block->live_in.erase(def_var->name);
        for (const std::shared_ptr<Backend::Variable> &used_var : instruction->get_used_variables())
            block->live_in.insert(used_var->name);
    }
    return changed || block->live_in.size() != old_in_size || block->live_out.size() != old_out_size;
}
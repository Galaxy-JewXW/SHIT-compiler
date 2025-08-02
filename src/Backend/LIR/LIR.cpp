#include "Backend/LIR/LIR.h"
#include "Backend/LIR/Instructions.h"

std::shared_ptr<Backend::Variable> Backend::LIR::Module::ensure_variable(const std::shared_ptr<Backend::Operand> &value, std::shared_ptr<Backend::LIR::Block> &block) {
    if (value->operand_type == OperandType::CONSTANT) {
        std::shared_ptr<Backend::Constant> constant = std::static_pointer_cast<Backend::Constant>(value);
        if (constant->constant_type == Backend::VariableType::INT32) {
            std::shared_ptr<Backend::Variable> temp_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("i32_const"), Backend::VariableType::INT32, VariableWide::LOCAL);
            block->instructions.push_back(std::make_shared<Backend::LIR::LoadIntImm>(temp_var, std::static_pointer_cast<Backend::IntValue>(constant)));
            block->parent_function.lock()->add_variable(temp_var);
            return temp_var;
        } else {
            std::string fname = "@" + Backend::Utils::unique_name("f.").substr(2);
            std::shared_ptr<Backend::DataSection::Variable> fvar = std::make_shared<Backend::DataSection::Variable>(fname, Backend::VariableType::FLOAT);
            global_data->global_variables[fname] = fvar;
            fvar->init_value = std::make_shared<Backend::DataSection::Variable::Constants>(std::vector<std::shared_ptr<Backend::Constant>>{std::static_pointer_cast<Backend::FloatValue>(constant)});
            fvar->read_only = true;
            std::shared_ptr<Backend::Variable> addr = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("faddr"), Backend::VariableType::FLOAT_PTR, VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(addr);
            block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(fvar, addr));
            std::shared_ptr<Backend::Variable> fimm = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("f32_const"), Backend::VariableType::FLOAT, VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(fimm);
            block->instructions.push_back(std::make_shared<Backend::LIR::LoadFloat>(addr, fimm));
            return fimm;
        }
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
                std::shared_ptr<Mir::Type::Type> mir_type = alloc->get_type();
                std::shared_ptr<Backend::Variable> var = std::make_shared<Backend::Variable>(alloc->get_name(), Backend::Utils::to_reference(Backend::Utils::llvm_to_riscv(*mir_type)), VariableWide::FUNCTIONAL);
                if (mir_type->as<Mir::Type::Pointer>()->get_contain_type()->is_array())
                    var->length = mir_type->as<Mir::Type::Pointer>()->get_contain_type()->as<Mir::Type::Array>()->get_flattened_size();
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

std::shared_ptr<Backend::Variable> Backend::LIR::Module::load_addr(const std::shared_ptr<Backend::Pointer> &load_from, std::shared_ptr<Backend::LIR::Block> &lir_block) {
    if (load_from->base->lifetime == VariableWide::GLOBAL) {
        std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(load_from->base->workload_type), VariableWide::LOCAL);
        lir_block->parent_function.lock()->add_variable(base);
        lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(load_from->base, base));
        load_from->base = base;
    }
    if (load_from->offset->operand_type == Backend::OperandType::CONSTANT) {
        if (Backend::Utils::is_12bit(std::static_pointer_cast<Backend::IntValue>(load_from->offset)->int32_value << 2))
            return load_from->base;
        else {
            std::shared_ptr<Backend::Variable> offset = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("offset"), VariableType::INT32, VariableWide::LOCAL);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadIntImm>(offset, std::static_pointer_cast<Backend::IntValue>(load_from->offset)));
            lir_block->parent_function.lock()->add_variable(offset);
            load_from->offset = offset;
        }
    }
    if (Backend::Utils::is_pointer(load_from->base->workload_type) || load_from->base->lifetime == VariableWide::LOCAL) {
        std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(load_from->base->workload_type), VariableWide::LOCAL);
        lir_block->parent_function.lock()->add_variable(base);
        std::shared_ptr<Backend::Variable> offset = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("offset"), VariableType::INT32, VariableWide::LOCAL);
        lir_block->parent_function.lock()->add_variable(offset);
        lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SHIFT_LEFT, std::static_pointer_cast<Backend::Variable>(load_from->offset), std::make_shared<Backend::IntValue>(2), offset));
        lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, load_from->base, offset, base));
        load_from->offset = std::make_shared<Backend::IntValue>(0);
        return base;
    } else {
        std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(load_from->base->workload_type), VariableWide::LOCAL);
        lir_block->parent_function.lock()->add_variable(base);
        lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(load_from->base, base));
        load_from->base = base;
        base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(load_from->base->workload_type), VariableWide::LOCAL);
        lir_block->parent_function.lock()->add_variable(base);
        std::shared_ptr<Backend::Variable> offset = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("offset"), VariableType::INT32, VariableWide::LOCAL);
        lir_block->parent_function.lock()->add_variable(offset);
        lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SHIFT_LEFT, std::static_pointer_cast<Backend::Variable>(load_from->offset), std::make_shared<Backend::IntValue>(2), offset));
        lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, load_from->base, offset, base));
        load_from->offset = std::make_shared<Backend::IntValue>(0);
        return base;
    }
}

template<typename StoreInst, Backend::VariableType PTR>
void Backend::LIR::Module::load_store_instruction(const std::shared_ptr<Backend::Variable> &store_to, const std::shared_ptr<Backend::Variable> &store_from, std::shared_ptr<Backend::LIR::Block> &lir_block) {
    if (store_to->lifetime == VariableWide::GLOBAL) {
        // (single) global variable
        std::shared_ptr<Backend::Variable> addr_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), PTR, VariableWide::LOCAL);
        lir_block->parent_function.lock()->add_variable(addr_var);
        lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(store_to, addr_var));
        lir_block->instructions.push_back(std::make_shared<StoreInst>(addr_var, store_from));
    } else if (store_to->var_type == Variable::Type::PTR) {
        std::shared_ptr<Backend::Pointer> ep = std::static_pointer_cast<Backend::Pointer>(store_to);
        lir_block->instructions.push_back(std::make_shared<StoreInst>(ep->base, store_from, std::static_pointer_cast<Backend::IntValue>(ep->offset)->int32_value * 4));
    } else {
        // allocated
        lir_block->instructions.push_back(std::make_shared<StoreInst>(store_to, store_from));
    }
}

template<typename LoadInst>
void Backend::LIR::Module::load_load_instruction(const std::shared_ptr<Backend::Variable> &load_from, const std::shared_ptr<Backend::Variable> &load_to, std::shared_ptr<Backend::LIR::Block> &lir_block) {
    if (load_from->var_type != Variable::Type::PTR) {
        // global variable or allocated variable
        if (load_from->lifetime == VariableWide::GLOBAL) {
            std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(load_from->workload_type), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(base);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(load_from, base));
            lir_block->instructions.push_back(std::make_shared<LoadInst>(base, load_to));
        } else lir_block->instructions.push_back(std::make_shared<LoadInst>(load_from, load_to));
    } else {
        // otherwise, load from an element pointer
        std::shared_ptr<Backend::Pointer> ep = std::static_pointer_cast<Backend::Pointer>(load_from);
        lir_block->instructions.push_back(std::make_shared<LoadInst>(ep->base, load_to, std::static_pointer_cast<Backend::IntValue>(ep->offset)->int32_value * 4));
    }
}

void Backend::LIR::Module::load_instruction(const std::shared_ptr<Mir::Instruction> &llvm_instruction, std::shared_ptr<Backend::LIR::Block> &lir_block) {
    switch (llvm_instruction->get_op()) {
        case Mir::Operator::MOVE: {
            std::shared_ptr<Mir::Move> move = std::static_pointer_cast<Mir::Move>(llvm_instruction);
            std::shared_ptr<Backend::Variable> move_from = ensure_variable(find_operand(move->get_from_value(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> move_to = find_variable(move->get_to_value()->get_name(), lir_block->parent_function.lock());
            if (!move_to) move_to = std::make_shared<Backend::Variable>(move->get_to_value()->get_name(), Backend::Utils::llvm_to_riscv(*move->get_to_value()->get_type()), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(move_to);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::Move>(move_from, move_to));
            break;
        }
        case Mir::Operator::LOAD: {
            std::shared_ptr<Mir::Load> load = std::static_pointer_cast<Mir::Load>(llvm_instruction);
            std::shared_ptr<Backend::Variable> load_from = find_variable(load->get_addr()->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> load_to = std::make_shared<Backend::Variable>(load->get_name(), Backend::Utils::llvm_to_riscv(*load->get_type()), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(load_to);
            if (Backend::Utils::is_int(load_to->workload_type))
                load_load_instruction<Backend::LIR::LoadInt>(load_from, load_to, lir_block);
            else
                load_load_instruction<Backend::LIR::LoadFloat>(load_from, load_to, lir_block);
            break;
        }
        case Mir::Operator::STORE: {
            std::shared_ptr<Mir::Store> store = std::static_pointer_cast<Mir::Store>(llvm_instruction);
            std::shared_ptr<Backend::Variable> store_to = find_variable(store->get_addr()->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> store_from = ensure_variable(find_operand(store->get_value(), lir_block->parent_function.lock()), lir_block);
            if (Backend::Utils::is_int(store_from->workload_type))
                load_store_instruction<Backend::LIR::StoreInt, Backend::VariableType::INT32_PTR>(store_to, store_from, lir_block);
            else
                load_store_instruction<Backend::LIR::StoreFloat, Backend::VariableType::FLOAT_PTR>(store_to, store_from, lir_block);
            break;
        }
        case Mir::Operator::GEP: {
            std::shared_ptr<Mir::GetElementPtr> get_element_pointer = std::static_pointer_cast<Mir::GetElementPtr>(llvm_instruction);
            std::shared_ptr<Backend::Variable> base_ = find_variable(get_element_pointer->get_addr()->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Operand> offset_ = find_operand(get_element_pointer->get_index(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Pointer> ep_ = std::make_shared<Backend::Pointer>(llvm_instruction->get_name(), base_, offset_);
            std::shared_ptr<Backend::Variable> base = load_addr(ep_, lir_block);
            std::shared_ptr<Backend::Pointer> ep = std::make_shared<Backend::Pointer>(llvm_instruction->get_name(), base, ep_->offset);
            lir_block->parent_function.lock()->add_variable(ep);
            break;
        }
        case Mir::Operator::FPTOSI: {
            std::shared_ptr<Mir::Fptosi> fptosi = std::static_pointer_cast<Mir::Fptosi>(llvm_instruction);
            std::shared_ptr<Backend::Variable> source = ensure_variable(find_operand(fptosi->get_value(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> dest = std::make_shared<Backend::Variable>(fptosi->get_name(), VariableType::INT32, VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(dest);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::Convert>(Backend::LIR::InstructionType::F2I, source, dest));
            break;
        }
        case Mir::Operator::SITOFP: {
            std::shared_ptr<Mir::Sitofp> sitofp = std::static_pointer_cast<Mir::Sitofp>(llvm_instruction);
            std::shared_ptr<Backend::Variable> source = ensure_variable(find_operand(sitofp->get_value(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> dest = std::make_shared<Backend::Variable>(sitofp->get_name(), VariableType::FLOAT, VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(dest);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::Convert>(Backend::LIR::InstructionType::I2F, source, dest));
            break;
        }
        case Mir::Operator::FCMP: {
            std::shared_ptr<Mir::Fcmp> fcmp = std::static_pointer_cast<Mir::Fcmp>(llvm_instruction);
            std::shared_ptr<Backend::Variable> lhs = ensure_variable(find_operand(fcmp->get_lhs(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> rhs = ensure_variable(find_operand(fcmp->get_rhs(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Variable>(fcmp->get_name(), VariableType::INT32, VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(result);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::FBranch>(Backend::Utils::llvm_to_lir(fcmp->op), lhs, rhs, result));
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
            if (cond_var->var_type == Backend::Variable::Type::OBJ) {
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::IBranch>(InstructionType::NOT_EQUAL_ZERO, cond_var, block_true));
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::Jump>(block_false));
                break;
            }
            if (cond_var->rhs->operand_type == OperandType::CONSTANT) {
                std::shared_ptr<Backend::Constant> rhs = std::static_pointer_cast<Backend::Constant>(cond_var->rhs);
                if (rhs->constant_type == VariableType::INT32 && std::static_pointer_cast<Backend::IntValue>(rhs)->int32_value == 0) {
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::IBranch>(Backend::Utils::cmp_to_lir_zero(cond_var->compare_type), cond_var->lhs, block_true));
                    lir_block->instructions.push_back(std::make_shared<Backend::LIR::Jump>(block_false));
                    break;
                }
            }
            std::shared_ptr<Backend::Variable> rhs = ensure_variable(cond_var->rhs, lir_block);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::IBranch>(Backend::Utils::cmp_to_lir(cond_var->compare_type), cond_var->lhs, rhs, block_true));
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::Jump>(block_false));
            break;
        }
        case Mir::Operator::BITCAST: {
            std::shared_ptr<Mir::BitCast> bitcast = std::static_pointer_cast<Mir::BitCast>(llvm_instruction);
            std::shared_ptr<Backend::Variable> src = find_variable(bitcast->get_value()->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> dst = std::make_shared<Backend::Pointer>(bitcast->get_name(), src);
            lir_block->parent_function.lock()->add_variable(dst);
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
            if (ret->get_value())
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::Return>(ensure_variable(find_operand(ret->get_value(), lir_block->parent_function.lock()), lir_block)));
            else
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::Return>());
            break;
        }
        case Mir::Operator::CALL: {
            lir_block->parent_function.lock()->is_caller = true;
            std::shared_ptr<Mir::Call> call = std::static_pointer_cast<Mir::Call>(llvm_instruction);
            std::string function_name = call->get_function()->get_name();
            std::vector<std::shared_ptr<Backend::Variable>> function_params;
            const std::vector<std::shared_ptr<Mir::Value>> llvm_params = call->get_params();
            if (function_name == "llvm.memset.p0i8.i32") {
                function_name = "memset";
                std::shared_ptr<Backend::Pointer> param1 = std::static_pointer_cast<Backend::Pointer>(find_variable(llvm_params[0]->get_name(), lir_block->parent_function.lock()));
                std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(param1->base->workload_type), VariableWide::LOCAL);
                lir_block->parent_function.lock()->add_variable(base);
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(param1->base, base));
                std::shared_ptr<Backend::Variable> zero = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("zero"), VariableType::INT32, VariableWide::LOCAL);
                lir_block->parent_function.lock()->add_variable(zero);
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadIntImm>(zero, std::make_shared<Backend::IntValue>(0)));
                std::shared_ptr<Backend::Variable> size = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("size"), VariableType::INT32, VariableWide::LOCAL);
                lir_block->parent_function.lock()->add_variable(size);
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadIntImm>(size, std::static_pointer_cast<Backend::IntValue>(find_operand(llvm_params[2], lir_block->parent_function.lock()))));
                function_params.push_back(base);
                function_params.push_back(zero);
                function_params.push_back(size);
            } else {
                for (std::shared_ptr<Mir::Value> param : llvm_params) {
                    std::shared_ptr<Backend::Variable> param_ = ensure_variable(find_operand(param, lir_block->parent_function.lock()), lir_block);
                    if (param_->var_type == Variable::Type::PTR) {
                        std::shared_ptr<Backend::Pointer> ep = std::make_shared<Backend::Pointer>(*std::static_pointer_cast<Backend::Pointer>(param_));
                        param_ = load_addr(ep, lir_block);
                        if (ep->base->lifetime == VariableWide::FUNCTIONAL) {
                            std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(ep->base->workload_type), VariableWide::LOCAL);
                            lir_block->parent_function.lock()->add_variable(base);
                            lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadAddress>(ep->base, base));
                            ep->base = base;
                            param_ = base;
                        }
                        if (std::static_pointer_cast<Backend::IntValue>(ep->offset)->int32_value) {
                            std::shared_ptr<Backend::Variable> base = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(ep->base->workload_type), VariableWide::LOCAL);
                            lir_block->parent_function.lock()->add_variable(base);
                            lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, ep->base, ep->offset, base));
                            ep->base = base;
                            param_ = base;
                        }
                    }
                    function_params.push_back(param_);
                }
            }
            if (!call->get_type()->is_void()) {
                std::shared_ptr<Backend::Variable> store_to = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*call->get_type()), VariableWide::LOCAL);
                lir_block->parent_function.lock()->add_variable(store_to);
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::Call>(store_to, functions_index[function_name], function_params));
            } else {
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::Call>(functions_index[function_name], function_params));
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
            } else if (lhs->operand_type == Backend::OperandType::CONSTANT && (int_operation_->op == Mir::IntBinary::Op::ADD || int_operation_->op == Mir::IntBinary::Op::MUL))
                std::swap(lhs, rhs);
            else lhs = ensure_variable(lhs, lir_block);
            if (int_operation_->op != Mir::IntBinary::Op::ADD && int_operation_->op != Mir::IntBinary::Op::SUB)
                rhs = ensure_variable(rhs, lir_block);
            else if (rhs->operand_type == Backend::OperandType::CONSTANT && !Backend::Utils::is_12bit(std::static_pointer_cast<Backend::IntValue>(rhs)->int32_value)) {
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::LoadIntImm>(result, std::static_pointer_cast<Backend::IntValue>(rhs)));
                lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::Utils::llvm_to_lir(int_operation_->op), std::static_pointer_cast<Backend::Variable>(lhs), result, result));
                break;
            }
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::Utils::llvm_to_lir(int_operation_->op), std::static_pointer_cast<Backend::Variable>(lhs), rhs, result));
            break;
        }
        case Mir::Operator::FLOATBINARY: {
            std::shared_ptr<Mir::FloatBinary> float_binary = std::static_pointer_cast<Mir::FloatBinary>(llvm_instruction);
            std::shared_ptr<Backend::Variable> lhs = ensure_variable(find_operand(float_binary->get_lhs(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> rhs = ensure_variable(find_operand(float_binary->get_rhs(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*float_binary->get_type()), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(result);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::FloatArithmetic>(Backend::Utils::llvm_to_lir(float_binary->op), lhs, rhs, result));
            break;
        }
        case Mir::Operator::FLOATTERNARY: {
            std::shared_ptr<Mir::FloatTernary> float_ternary = std::static_pointer_cast<Mir::FloatTernary>(llvm_instruction);
            std::shared_ptr<Backend::Variable> hs = ensure_variable(find_operand(float_ternary->get_x(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> ht = ensure_variable(find_operand(float_ternary->get_y(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> hu = ensure_variable(find_operand(float_ternary->get_z(), lir_block->parent_function.lock()), lir_block);
            std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*float_ternary->get_type()), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(result);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::FloatTernary>(Backend::Utils::llvm_to_lir(float_ternary->op), hs, ht, hu, result));
            break;
        }
        case Mir::Operator::FNEG: {
            std::shared_ptr<Mir::FNeg> fneg = std::static_pointer_cast<Mir::FNeg>(llvm_instruction);
            std::shared_ptr<Backend::Variable> operand = find_variable(fneg->get_value()->get_name(), lir_block->parent_function.lock());
            std::shared_ptr<Backend::Variable> result = std::make_shared<Backend::Variable>(llvm_instruction->get_name(), Backend::Utils::llvm_to_riscv(*fneg->get_type()), VariableWide::LOCAL);
            lir_block->parent_function.lock()->add_variable(result);
            lir_block->instructions.push_back(std::make_shared<Backend::LIR::FNeg>(operand, result));
            break;
        }
        default: break;
    }
}

template <typename T_store, typename T_load>
void Backend::LIR::Function::spill(std::shared_ptr<Backend::Variable> &local_variable) {
    if (local_variable->lifetime != VariableWide::LOCAL)
        log_error("Only variable in register can be spilled.");
    for (std::shared_ptr<Backend::LIR::Block> &block : blocks) {
        for (size_t i = 0; i < block->instructions.size(); i++) {
            std::shared_ptr<Backend::LIR::Instruction> &instr = block->instructions[i];
            std::vector<std::shared_ptr<Backend::Variable>> used = instr->get_used_variables();
            if (instr->get_defined_variable() && *instr->get_defined_variable() == *local_variable) {
                // insert `store` after the instruction
                std::shared_ptr<Backend::Variable> new_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("spill_"), local_variable->workload_type, VariableWide::LOCAL);
                add_variable(new_var);
                log_debug("Spilling `%s` to `%s` in `%s`", local_variable->name.c_str(), new_var->name.c_str(), instr->to_string().c_str());
                instr->update_defined_variable(new_var);
                block->instructions.insert(block->instructions.begin() + i + 1, std::make_shared<T_store>(local_variable, new_var));
                i++;
            } else if (std::find(used.begin(), used.end(), local_variable) != used.end()) {
                // insert `load` before the instruction
                std::shared_ptr<Backend::Variable> new_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("spill_"), local_variable->workload_type, VariableWide::LOCAL);
                add_variable(new_var);
                log_debug("Loading spilled `%s` to `%s` in `%s`", local_variable->name.c_str(), new_var->name.c_str(), instr->to_string().c_str());
                instr->update_used_variable(local_variable, new_var);
                block->instructions.insert(block->instructions.begin() + i, std::make_shared<T_load>(local_variable, new_var));
                i++;
            }
        }
    }
    local_variable->lifetime = VariableWide::FUNCTIONAL;
}

template void Backend::LIR::Function::spill<Backend::LIR::StoreInt, Backend::LIR::LoadInt>(std::shared_ptr<Backend::Variable> &local_variable);
template void Backend::LIR::Function::spill<Backend::LIR::StoreFloat, Backend::LIR::LoadFloat>(std::shared_ptr<Backend::Variable> &local_variable);

template<bool (*is_consistent)(const Backend::VariableType &type)>
void Backend::LIR::Function::analyze_live_variables() {
    bool changed = true;
    for (std::shared_ptr<Backend::LIR::Block> &block : blocks) {
        block->live_in.clear();
        block->live_out.clear();
    }
    while(changed) {
        std::unordered_set<std::string> visited;
        std::shared_ptr<Backend::LIR::Block> first_block = blocks.front();
        changed = analyze_live_variables<is_consistent>(first_block, visited);
    }
    // std::cout << live_variables();
}

template void Backend::LIR::Function::analyze_live_variables<Backend::Utils::is_int>();
template void Backend::LIR::Function::analyze_live_variables<Backend::Utils::is_float>();

template<bool (*is_consistent)(const Backend::VariableType &type)>
bool Backend::LIR::Function::analyze_live_variables(std::shared_ptr<Backend::LIR::Block> &block, std::unordered_set<std::string> &visited) {
    bool changed = false;
    size_t old_in_size = block->live_in.size();
    size_t old_out_size = block->live_out.size();
    visited.insert(block->name);
    // live_out = sum(live_in)
    for (std::shared_ptr<Backend::LIR::Block> &succ : block->successors) {
        if (visited.find(succ->name) == visited.end())
            changed = analyze_live_variables<is_consistent>(succ, visited) | changed;
        block->live_out.insert(succ->live_in.begin(), succ->live_in.end());
    }
    // live_in = (live_out - def) + use
    block->live_in.insert(block->live_out.begin(), block->live_out.end());
    for (std::vector<std::shared_ptr<Backend::LIR::Instruction>>::reverse_iterator it = block->instructions.rbegin(); it != block->instructions.rend(); it++) {
        std::shared_ptr<Backend::LIR::Instruction> &instruction = *it;
        std::shared_ptr<Backend::Variable> def_var = instruction->get_defined_variable(is_consistent);
        if (def_var)
            block->live_in.erase(def_var->name);
        for (const std::shared_ptr<Backend::Variable> &used_var : instruction->get_used_variables(is_consistent))
            if (is_consistent(used_var->workload_type))
                block->live_in.insert(used_var->name);
    }
    return changed || block->live_in.size() != old_in_size || block->live_out.size() != old_out_size;
}

template bool Backend::LIR::Function::analyze_live_variables<Backend::Utils::is_int>(std::shared_ptr<Backend::LIR::Block> &block, std::unordered_set<std::string> &visited);
template bool Backend::LIR::Function::analyze_live_variables<Backend::Utils::is_float>(std::shared_ptr<Backend::LIR::Block> &block, std::unordered_set<std::string> &visited);
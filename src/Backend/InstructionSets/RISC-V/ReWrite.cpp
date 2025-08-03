#include "Backend/InstructionSets/RISC-V/ReWrite.h"

void RISCV::ReWrite::create_entry_block(const std::shared_ptr<Backend::LIR::Function> &lir_function) {
    std::shared_ptr<Backend::LIR::Block> first_block = lir_function->blocks.front();
    if (first_block->name == BLOCK_ENTRY)
        return;
    std::shared_ptr<Backend::LIR::Block> block_entry = std::make_shared<Backend::LIR::Block>(BLOCK_ENTRY);
    block_entry->parent_function = lir_function;
    block_entry->successors.push_back(first_block);
    first_block->predecessors.push_back(block_entry);
    lir_function->blocks_index[block_entry->name] = block_entry;
    lir_function->blocks.insert(lir_function->blocks.begin(), block_entry);
}

void RISCV::ReWrite::rewrite_large_offset(const std::shared_ptr<Backend::LIR::Function> &lir_function, const std::shared_ptr<RISCV::Stack> &stack) {
    for (std::shared_ptr<Backend::LIR::Block> block: lir_function->blocks) {
        for (size_t i = 0; i < block->instructions.size(); ++i) {
            std::shared_ptr<Backend::LIR::Instruction> instruction = block->instructions[i];
            if (instruction->type == Backend::LIR::InstructionType::LOAD) {
                std::shared_ptr<Backend::LIR::LoadInt> instruction_ = std::static_pointer_cast<Backend::LIR::LoadInt>(instruction);
                std::shared_ptr<Backend::Variable> var_in_mem = instruction_->var_in_mem;
                if (var_in_mem->lifetime == Backend::VariableWide::FUNCTIONAL) {
                    int32_t offset = stack->offset_of(var_in_mem);
                    if (Backend::Utils::is_12bit(offset))
                        continue;
                    std::shared_ptr<Backend::Variable> addr = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(var_in_mem->workload_type), Backend::VariableWide::LOCAL);
                    lir_function->add_variable(addr);
                    block->instructions.insert(block->instructions.begin() + i, std::make_shared<Backend::LIR::LoadAddress>(var_in_mem, addr));
                    instruction_->var_in_mem = addr;
                    if (instruction_->offset)
                        log_warn("Offset is not zero!");
                }
            } else if (instruction->type == Backend::LIR::InstructionType::STORE) {
                std::shared_ptr<Backend::LIR::StoreInt> instruction_ = std::static_pointer_cast<Backend::LIR::StoreInt>(instruction);
                std::shared_ptr<Backend::Variable> var_in_mem = instruction_->var_in_mem;
                if (var_in_mem->lifetime == Backend::VariableWide::FUNCTIONAL) {
                    int32_t offset = stack->offset_of(var_in_mem);
                    if (Backend::Utils::is_12bit(offset))
                        continue;
                    std::shared_ptr<Backend::Variable> addr = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(var_in_mem->workload_type), Backend::VariableWide::LOCAL);
                    lir_function->add_variable(addr);
                    block->instructions.insert(block->instructions.begin() + i, std::make_shared<Backend::LIR::LoadAddress>(var_in_mem, addr));
                    instruction_->var_in_mem = addr;
                    if (instruction_->offset)
                        log_warn("Offset is not zero!");
                }
            }
        }
    }
}

void RISCV::ReWrite::rewrite_parameters_i(const std::shared_ptr<Backend::LIR::Function> &lir_function, const std::shared_ptr<RISCV::Stack> &stack) {
    std::shared_ptr<Backend::LIR::Block> block_entry = lir_function->blocks.front();
    for (size_t i = 0, j = 0; i < lir_function->parameters.size(); i++) {
        const std::shared_ptr<Backend::Variable> &arg = lir_function->parameters[i];
        if (!Backend::Utils::is_int(arg->workload_type))
            continue;
        if (j < 8)
            block_entry->instructions.push_back(std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::A0 + j++)], arg));
        else
            block_entry->instructions.push_back(std::make_shared<Backend::LIR::LoadInt>(lir_function->variables[arg->name + "_mem"], arg));
    }
    for (const std::shared_ptr<Backend::LIR::Block> &block : lir_function->blocks) {
        for (size_t i = 0; i < block->instructions.size(); i++) {
            std::shared_ptr<Backend::LIR::Instruction> instruction = block->instructions[i];
            if (instruction->type == Backend::LIR::InstructionType::CALL) {
                std::shared_ptr<Backend::LIR::Call> call = std::static_pointer_cast<Backend::LIR::Call>(block->instructions[i]);
                for (size_t j = 0, k = 0, sp_offset = RISCV::Stack::RA_SIZE; j < call->arguments.size(); j++) {
                    const std::shared_ptr<Backend::Variable> &arg = call->arguments[j];
                    if (Backend::Utils::is_int(arg->workload_type)) {
                        sp_offset += Backend::Utils::type_to_size(arg->workload_type);
                        if (k < 8) {
                            // move arguments to a0-a7
                            const std::shared_ptr<Backend::Variable> &phyReg = lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::A0 + k++)];
                            block->instructions.insert(block->instructions.begin() + i++, std::make_shared<Backend::LIR::Move>(call->arguments[j], phyReg));
                            call->arguments[j] = phyReg;
                        } else {
                            std::shared_ptr<Backend::Variable> param_ = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("param"), arg->workload_type, Backend::VariableWide::FUNCTIONAL);
                            stack->add_parameter(param_, sp_offset);
                            size_t instert_at = i;
                            for (size_t i_ = i; i_ >= 0; i_--) {
                                if (block->instructions[i_]->get_defined_variable() == arg) {
                                    instert_at = i_;
                                    break;
                                }
                            }
                            block->instructions.insert(block->instructions.begin() + instert_at, std::make_shared<Backend::LIR::StoreInt>(param_, arg));
                            call->arguments[j] = param_;
                            i++;
                        }
                    }
                }
                // move result of the call to a0
                if (call->result && Backend::Utils::is_int(call->result->workload_type))
                    block->instructions.insert(block->instructions.begin() + i + 1, std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::A0)], call->result)),
                    call->result = lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::A0)];
            }
        }
    }
}

void RISCV::ReWrite::rewrite_parameters_f(const std::shared_ptr<Backend::LIR::Function> &lir_function, const std::shared_ptr<RISCV::Stack> &stack) {
    std::shared_ptr<Backend::LIR::Block> block_entry = lir_function->blocks.front();
    for (size_t i = 0, j = 0; i < lir_function->parameters.size(); i++) {
        const std::shared_ptr<Backend::Variable> &arg = lir_function->parameters[i];
        if (!Backend::Utils::is_float(arg->workload_type))
            continue;
        if (j < 8)
            block_entry->instructions.push_back(std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0 + j++)], arg));
        else
            block_entry->instructions.push_back(std::make_shared<Backend::LIR::LoadFloat>(lir_function->variables[arg->name + "_mem"], arg));
    }
    for (const std::shared_ptr<Backend::LIR::Block> &block : lir_function->blocks) {
        for (size_t i = 0; i < block->instructions.size(); i++) {
            std::shared_ptr<Backend::LIR::Instruction> instruction = block->instructions[i];
            if (instruction->type == Backend::LIR::InstructionType::CALL) {
                std::shared_ptr<Backend::LIR::Call> call = std::static_pointer_cast<Backend::LIR::Call>(block->instructions[i]);
                for (size_t j = 0, k = 0, sp_offset = RISCV::Stack::RA_SIZE; j < call->arguments.size(); j++) {
                    const std::shared_ptr<Backend::Variable> &arg = call->arguments[j];
                    if (Backend::Utils::is_float(arg->workload_type)) {
                        sp_offset += Backend::Utils::type_to_size(arg->workload_type);
                        if (k < 8) {
                            // move arguments to fa0-fa7
                            const std::shared_ptr<Backend::Variable> &phyReg = lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0 + k++)];
                            block->instructions.insert(block->instructions.begin() + i++, std::make_shared<Backend::LIR::Move>(call->arguments[j], phyReg));
                            call->arguments[j] = phyReg;
                        } else {
                            std::shared_ptr<Backend::Variable> param_ = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("param"), arg->workload_type, Backend::VariableWide::FUNCTIONAL);
                            stack->add_parameter(param_, sp_offset);
                            block->instructions.insert(block->instructions.begin() + i++, std::make_shared<Backend::LIR::StoreFloat>(param_, arg));
                            call->arguments[j] = param_;
                        }
                    }
                }
                // move result of the call to fa0
                if (call->result && Backend::Utils::is_float(call->result->workload_type))
                    block->instructions.insert(block->instructions.begin() + i + 1, std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0)], call->result)),
                    call->result = lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0)];
            }
        }
    }
}
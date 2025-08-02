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
        for (std::vector<std::shared_ptr<Backend::LIR::Instruction>>::iterator it = block->instructions.begin(); it != block->instructions.end(); it++) {
            std::shared_ptr<Backend::LIR::Instruction> instruction = *it;
            if (instruction->type == Backend::LIR::InstructionType::LOAD) {
                std::shared_ptr<Backend::LIR::LoadInt> instruction_ = std::static_pointer_cast<Backend::LIR::LoadInt>(instruction);
                std::shared_ptr<Backend::Variable> var_in_mem = instruction_->var_in_mem;
                if (var_in_mem->lifetime == Backend::VariableWide::FUNCTIONAL) {
                    int32_t offset = stack->offset_of(var_in_mem);
                    if (Backend::Utils::is_12bit(offset))
                        continue;
                    std::shared_ptr<Backend::Variable> addr = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("addr"), Backend::Utils::to_pointer(var_in_mem->workload_type), Backend::VariableWide::LOCAL);
                    block->instructions.insert(it, std::make_shared<Backend::LIR::LoadAddress>(var_in_mem, addr));
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
                    block->instructions.insert(it, std::make_shared<Backend::LIR::LoadAddress>(var_in_mem, addr));
                    instruction_->var_in_mem = addr;
                    if (instruction_->offset)
                        log_warn("Offset is not zero!");
                }
            }
        }
    }
}
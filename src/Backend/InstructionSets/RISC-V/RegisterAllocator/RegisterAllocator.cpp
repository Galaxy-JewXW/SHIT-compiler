#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/LinearScan.h"
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/GraphColoring.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Utils/Log.h"

RISCV::RegisterAllocator::Allocator::Allocator(const std::shared_ptr<Backend::LIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack) : stack(stack), mir_function(function) {}

std::shared_ptr<RISCV::RegisterAllocator::Allocator> RISCV::RegisterAllocator::create(AllocationType type, const std::shared_ptr<Backend::LIR::Function> &function, std::shared_ptr<RISCV::Stack> &stack) {
    switch (type) {
        case AllocationType::GRAPH_COLORING:
            return std::make_shared<RISCV::RegisterAllocator::GraphColoring>(function, stack);
        default:
            log_error("Unsupported register allocation type");
            return nullptr;
    }
}

RISCV::Registers::ABI RISCV::RegisterAllocator::Allocator::get_register(const std::shared_ptr<Backend::Variable> &variable) const {
    if (var_to_reg.find(variable->name) != var_to_reg.end())
        return var_to_reg.at(variable->name);
    return RISCV::Registers::ABI::ZERO;
}

void RISCV::Utils::analyze_live_variables(std::shared_ptr<Backend::LIR::Function> &function) {
    bool changed = true;
    while(changed) {
        std::vector<std::string> visited;
        std::shared_ptr<Backend::LIR::Block> first_block = function->blocks.front();
        changed = RISCV::Utils::analyze_live_variables(first_block, visited);
    }
}

bool RISCV::Utils::analyze_live_variables(std::shared_ptr<Backend::LIR::Block> &block, std::vector<std::string> &visited) {
    bool changed = false;
    size_t old_in_size = block->live_in.size();
    size_t old_out_size = block->live_out.size();
    visited.push_back(block->name);
    // live_out = sum(live_in)
    for (std::shared_ptr<Backend::LIR::Block> &succ : block->successors) {
        if (std::find(visited.begin(), visited.end(), succ->name) == visited.end()) {
            changed = changed || RISCV::Utils::analyze_live_variables(succ, visited);
            block->live_out.insert(succ->live_in.begin(), succ->live_in.end());
        }
    }
    // live_in = (live_out - def) + use
    block->live_in.insert(block->live_out.begin(), block->live_out.end());
    for (std::vector<std::shared_ptr<Backend::LIR::Instruction>>::reverse_iterator it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
        std::shared_ptr<Backend::LIR::Instruction> &instruction = *it;
        std::shared_ptr<Backend::Variable> def_var = instruction->get_defined_variable();
        if (def_var)
            block->live_in.erase(def_var);
        std::vector<std::shared_ptr<Backend::Variable>> used_vars = instruction->get_used_variables();
        for (const std::shared_ptr<Backend::Variable> &var : used_vars)
            block->live_in.insert(var);
    }
    return changed || block->live_in.size() != old_in_size || block->live_out.size() != old_out_size;
}
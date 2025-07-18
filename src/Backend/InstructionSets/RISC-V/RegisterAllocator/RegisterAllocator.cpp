#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/GraphColoring.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"

RISCV::RegisterAllocator::Allocator::Allocator(const std::shared_ptr<Backend::LIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack) : stack(stack), lir_function(function) {}

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

std::string RISCV::RegisterAllocator::Allocator::to_string() const {
    std::ostringstream oss;
    oss << "Register Allocator for function: " << lir_function->name << "\n";
    for (const auto& [var_name, var] : lir_function->variables) {
        if (var_to_reg.find(var_name) != var_to_reg.end()) {
            oss << "  " << var_name << " -> " << RISCV::Registers::to_string(var_to_reg.at(var_name)) << "\n";
        } else if (stack->stack_index.find(var_name) != stack->stack_index.end()) {
            oss << "  " << var_name << " -> stack\n";
        } else {
            log_error("Variable %s not found in register allocation map or stack index", var_name.c_str());
        }
    }
    return oss.str();
}

void RISCV::Utils::analyze_live_variables(std::shared_ptr<Backend::LIR::Function> &function) {
    bool changed = true;
    while(changed) {
        std::vector<std::string> visited;
        std::shared_ptr<Backend::LIR::Block> first_block = function->blocks.front();
        changed = RISCV::Utils::analyze_live_variables(first_block, visited);
    }
    // std::ostringstream oss;
    // for (const auto& block: function->blocks) {
    //     oss << "\nBlock: " << block->name << "\n";
    //     oss << "  Live In: ";
    //     for (const auto& var : block->live_in) {
    //         oss << var->name << " ";
    //     }
    //     oss << "\n";
    //     oss << "  Live Out: ";
    //     for (const auto& var : block->live_out) {
    //         oss << var->name << " ";
    //     }
    // }
    // log_debug(oss.str().c_str());
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
        for (const std::shared_ptr<Backend::Variable> &used_var : instruction->get_used_variables())
            block->live_in.insert(used_var);
    }
    return changed || block->live_in.size() != old_in_size || block->live_out.size() != old_out_size;
}
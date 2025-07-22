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
    oss << "\nRegister Allocator for function: " << lir_function->name << "\n";
    for (const std::pair<std::string, std::shared_ptr<Backend::Variable>> name_var : lir_function->variables)
        if(name_var.first.find('%') != 0) // skip physical registers
            continue;
        else if (var_to_reg.find(name_var.first) != var_to_reg.end())
            oss << "  " << name_var.first << " -> " << RISCV::Registers::to_string(var_to_reg.at(name_var.first)) << "\n";
        else if (stack->stack_index.find(name_var.first) != stack->stack_index.end())
            oss << "  " << name_var.first << " -> stack\n";
        else log_error("Variable %s not found in register allocation map or stack index", name_var.first.c_str());
    return oss.str();
}
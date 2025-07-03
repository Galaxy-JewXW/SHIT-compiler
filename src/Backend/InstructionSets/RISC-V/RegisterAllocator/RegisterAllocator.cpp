#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/LinearScan.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Utils/Log.h"

std::shared_ptr<RISCV::RegisterAllocator::Allocator> RISCV::RegisterAllocator::create(AllocationType type, const std::shared_ptr<Backend::MIR::Function> &function, std::shared_ptr<RISCV::Stack> &stack) {
    switch (type) {
        case AllocationType::LINEAR_SCAN:
            return std::make_shared<RISCV::RegisterAllocator::LinearScan>(function, stack);
        default:
            log_error("Unsupported register allocation type");
            return nullptr;
    }
}

RISCV::Registers::ABI RISCV::RegisterAllocator::Allocator::get_register(const std::shared_ptr<Backend::MIR::Variable> &variable) const {
    if (var_to_reg.find(variable->name) != var_to_reg.end())
        return var_to_reg.at(variable->name);
    return RISCV::Registers::ABI::ZERO;
}
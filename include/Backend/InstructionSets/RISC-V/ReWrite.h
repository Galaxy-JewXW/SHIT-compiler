#ifndef RV_REWRITE_H
#define RV_REWRITE_H

#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/LIR/Instructions.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/LIR/LIR.h"
#include "Backend/VariableTypes.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Utils/Log.h"

#define BLOCK_ENTRY "block_entry"

namespace RISCV::ReWrite {
    void create_entry_block(const std::shared_ptr<Backend::LIR::Function> &lir_function);
    void rewrite_large_offset(const std::shared_ptr<Backend::LIR::Function> &lir_function, const std::shared_ptr<RISCV::Stack> &stack);
}

#endif
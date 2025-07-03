#include "Backend/InstructionSets/RISC-V/Assembler.h"
#include "Utils/Log.h"

std::string RISCV::Assembler::to_string() const {
    return rv_module->to_string();
}
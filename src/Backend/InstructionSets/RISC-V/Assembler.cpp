#include "Backend/InstructionSets/RISC-V/Assembler.h"
#include "Utils/Log.h"

[[nodiscard]] std::string RISCV::Assembler::to_string() const {
    std::ostringstream oss;
    oss << this->mir_module->to_string();
    return oss.str();
}
#include "Backend/Instructions/RISC-V/Instruction.h"

[[nodiscard]] std::string RISCV_Instructions::TextField::to_string() const {
    std::ostringstream oss;
    oss << ".section .text\n.global main\n";
    return oss.str();
}

[[nodiscard]] std::string RISCV_Instructions::DataField::to_string() const {
    std::ostringstream oss;
    oss << ".section .rodata\n";
    size_t len = this->const_strings.size();
    for (size_t i = 1; i <= len; i++) {
        oss << "@.str_" << i << " .string \"" << this->const_strings[i - 1] << "\"\n";
    }
    oss << ".section .data\n";
    return oss.str();
}

[[nodiscard]] std::string RISCV_Instructions::FunctionField::to_string() const {
    std::ostringstream oss;
    oss << this->function_name << ":\n";
    // for (const auto &instruction : this->instructions) {
    //     oss << instruction->to_string() << "\n";
    // }
    return oss.str();
}

[[nodiscard]] std::string RISCV_Instructions::Instruction::to_string() const {
    return "";
}
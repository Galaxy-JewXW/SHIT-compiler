#include "Backend/Assembler.h"

Assembler::RISCV_Assembler::RISCV_Assembler(std::shared_ptr<Mir::Module> module) {
    this->module = module;
    this->data.const_strings = *module->get_const_strings();
    std::shared_ptr<Mir::Function> main_function = module->get_main_function();
    // TODO: Implement the constructor
}

std::string Assembler::RISCV_Assembler::to_string() const {
    std::ostringstream oss;
    oss << data.to_string();
    oss << "\n";
    oss << text.to_string();
    return oss.str();
}
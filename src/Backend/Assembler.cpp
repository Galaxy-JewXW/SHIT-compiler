#include "Backend/Assembler.h"

Assembler::RISCV_Assembler::RISCV_Assembler(std::shared_ptr<Mir::Module> module) {
    this->module = module;
    this->data.const_strings = *module->get_const_strings();
    this->data.load_global_variables(module->get_global_variables());
    for (const std::shared_ptr<Mir::Function> &function : module->get_functions()) {
        RISCV::Modules::FunctionField function_field(function->get_name());
        RISCV::Instructions::InstructionFactory::alloc_all(function, function_field);
        function_field.instructions.push_back(std::make_shared<RISCV::Instructions::AddImmediate>(function_field.sp, function_field.sp, -function_field.sp->offset));
        for (const std::shared_ptr<Mir::Block> &block : function->get_blocks()) {
            function_field.instructions.push_back(std::make_shared<RISCV::Instructions::Label>("." + function->get_name() + "." + block->get_name()));
            for (const std::shared_ptr<Mir::Instruction> &instruction : block->get_instructions()) {
                RISCV::Instructions::InstructionFactory::create(instruction, function_field);
            }
        }
        this->text.add_function(function_field);
    }
}

std::string Assembler::RISCV_Assembler::to_string() const {
    std::ostringstream oss;
    oss << data.to_string();
    oss << "\n";
    oss << text.to_string();
    return oss.str();
}
#ifndef RV_MODULES_H
#define RV_MODULES_H

#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include "Backend/Instructions/RISC-V/Variables.h"
#include "Backend/Instructions/RISC-V/Memory.h"
#include "Backend/Instructions/RISC-V/Instructions.h"
#include "Backend/Instructions/RISC-V/Registers.h"
#include "Mir/Structure.h"

namespace RISCV::Modules {
    class FunctionField {
        public:
            std::string function_name;
            std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> instructions;
            std::shared_ptr<RISCV::Memory::Memory> memory{std::make_shared<RISCV::Memory::Memory>(this)};
            std::shared_ptr<RISCV::Registers::StackPointer> sp{std::make_shared<RISCV::Registers::StackPointer>(this)};

            [[nodiscard]] std::string to_string() const;

            void add_instruction(const std::shared_ptr<RISCV::Instructions::Instruction> &instruction);

            explicit FunctionField(std::string function_name) : function_name(std::move(function_name)) {}
    };

    class TextField {
        private:
            std::vector<std::shared_ptr<FunctionField>>functions;
        public:
            [[nodiscard]] std::string to_string() const;

            void add_function(const FunctionField &function);
    };

    class DataField {
        public:
            std::vector<std::string> const_strings;
            std::vector<std::shared_ptr<RISCV::Variables::Variable>> global_variables;

            [[nodiscard]] std::string to_string() const;

            void load_global_variables(const std::vector<std::shared_ptr<Mir::GlobalVariable>> &global_variables);
    };
}

namespace RISCV::Instructions::InstructionFactory {
    void alloc_all(const std::shared_ptr<Mir::Function> &function, RISCV::Modules::FunctionField &function_field);

    void create(const std::shared_ptr<Mir::Instruction> &instruction, RISCV::Modules::FunctionField &function_field);

    template <typename T>
    void transform_integer_binary(const std::shared_ptr<Mir::IntBinary> &instruction, RISCV::Modules::FunctionField &function_field);

    template <typename T>
    void transform_integer_binary(const std::shared_ptr<Mir::Icmp> &instruction, RISCV::Modules::FunctionField &function_field);
}
#endif
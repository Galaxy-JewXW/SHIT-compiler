#ifndef RV_MEM_H
#define RV_MEM_H

#include "Mir/Instruction.h"
#include "Backend/Instructions/RISC-V/Registers.h"
#include "Backend/Instructions/RISC-V/Instructions.h"
#include <string>
#include <map>

#pragma once
namespace RISCV::Modules {
    class FunctionField;
}

namespace RISCV::Memory {
    class Memory {
        public:
            RISCV::Modules::FunctionField *function_field{nullptr};
            std::map<std::string, int64_t> vreg2offset;
            std::map<std::string, int64_t> vptr2offset;

            explicit Memory(RISCV::Modules::FunctionField *function_field) : function_field{function_field} {}
            ~Memory() = default;

            void load_to(const std::string &load_from, const std::string &load_to);

            void store_to(const std::string &store_to, std::shared_ptr<Mir::Value> value);

            void load_to(std::shared_ptr<RISCV::Registers::Register> reg, std::shared_ptr<Mir::Value> value);

            void store_to(const std::string &store_to, std::shared_ptr<RISCV::Registers::Register> reg);

            void get_element_pointer(const std::string &array, std::shared_ptr<Mir::Value> offset, const std::string &store_to);
    };
}

#endif
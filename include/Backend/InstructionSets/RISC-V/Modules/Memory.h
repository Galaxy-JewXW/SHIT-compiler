#ifndef RV_MEM_H
#define RV_MEM_H

#include "Mir/Instruction.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/InstructionSets/RISC-V/Modules/Instructions.h"
#include <string>
#include <map>
#include "Mir/Value.h"
#include "Utils/Log.h"

#pragma once
namespace RISCV::Modules {
    class Function;
}

namespace RISCV::Memory {
    class Memory {
        public:
            ~Memory() = default;

            void load_to(const std::shared_ptr<Backend::MIR::Variable> &load_from, const std::shared_ptr<Backend::MIR::Variable> &load_to);

            void store_to(const std::shared_ptr<Backend::MIR::Variable> &store_to, std::shared_ptr<Mir::Value> value);

            void load_to(std::shared_ptr<RISCV::Registers::ABI> reg, std::shared_ptr<Mir::Value> value);

            void store_to(const std::shared_ptr<Backend::MIR::Variable> &store_to, RISCV::Registers::ABI reg);

            void get_element_pointer(const std::shared_ptr<Backend::MIR::Variable> &array, std::shared_ptr<Mir::Value> offset, const std::shared_ptr<Backend::MIR::Variable> &store_to);

            RISCV::Registers::ABI get_register(const std::shared_ptr<Backend::MIR::Variable> &var) const;

            void load(const std::shared_ptr<Backend::MIR::Variable> &load_from);

            void store(const std::shared_ptr<Backend::MIR::Variable> &store_to);
    };
}

#endif
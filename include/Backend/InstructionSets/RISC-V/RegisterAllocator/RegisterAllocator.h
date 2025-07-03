#ifndef RV_REGISTER_ALLOCATOR_H
#define RV_REGISTER_ALLOCATOR_H

#include <memory>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <queue>
#include "Backend/MIR/MIR.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/InstructionSets/RISC-V/Instructions.h"

namespace RISCV::RegisterAllocator {
    class Allocator;
    class LinearScan;
    class GraphColoring;
    enum class AllocationType {
        LINEAR_SCAN,
        GRAPH_COLORING
    };
    std::shared_ptr<Allocator> create(AllocationType type, const std::shared_ptr<Backend::MIR::Function> &function, std::shared_ptr<RISCV::Stack> &stack);
}

namespace RISCV {
    class Stack;
}

class RISCV::RegisterAllocator::Allocator : std::enable_shared_from_this<Allocator> {
    public:
        explicit Allocator(const std::shared_ptr<Backend::MIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack)
            : stack(stack), mir_function(function) {};
        virtual void allocate() = 0;
        RISCV::Registers::ABI get_register(const std::shared_ptr<Backend::MIR::Variable>& variable) const;
        virtual RISCV::Registers::ABI use_register(const std::shared_ptr<Backend::MIR::Variable>& variable, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>>& instrs) = 0;
    protected:
        std::shared_ptr<RISCV::Stack> stack{nullptr};
        std::shared_ptr<Backend::MIR::Function> mir_function{nullptr};
        std::unordered_map<std::string, RISCV::Registers::ABI> var_to_reg;
        std::vector<RISCV::Registers::ABI> available_temp_regs = {
            RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T1, RISCV::Registers::ABI::T2,
            RISCV::Registers::ABI::T3, RISCV::Registers::ABI::T4, RISCV::Registers::ABI::T5,
            RISCV::Registers::ABI::T6
        };
        std::unordered_map<RISCV::Registers::ABI, std::shared_ptr<Backend::MIR::Variable>> var_in_treg;
        std::vector<RISCV::Registers::ABI> available_global_regs = {
            RISCV::Registers::ABI::A0, RISCV::Registers::ABI::A1, RISCV::Registers::ABI::A2, RISCV::Registers::ABI::A3,
            RISCV::Registers::ABI::A4, RISCV::Registers::ABI::A5, RISCV::Registers::ABI::A6, RISCV::Registers::ABI::A7,
            RISCV::Registers::ABI::S0, RISCV::Registers::ABI::S1, RISCV::Registers::ABI::S2, RISCV::Registers::ABI::S3,
            RISCV::Registers::ABI::S4, RISCV::Registers::ABI::S5, RISCV::Registers::ABI::S6, RISCV::Registers::ABI::S7,
            RISCV::Registers::ABI::S8, RISCV::Registers::ABI::S9, RISCV::Registers::ABI::S10, RISCV::Registers::ABI::S11
        };
};

#endif
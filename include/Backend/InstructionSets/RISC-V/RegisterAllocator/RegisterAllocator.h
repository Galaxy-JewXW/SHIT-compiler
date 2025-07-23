#ifndef RV_REGISTER_ALLOCATOR_H
#define RV_REGISTER_ALLOCATOR_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <array>
#include "Backend/LIR/LIR.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Utils/Log.h"

namespace RISCV {
    class Stack;
}

namespace RISCV::RegisterAllocator {
    class Allocator;
    class LinearScan;
    class GraphColoring;
    enum class AllocationType {
        LINEAR_SCAN,
        GRAPH_COLORING
    };
    std::shared_ptr<Allocator> create(AllocationType type, const std::shared_ptr<Backend::LIR::Function> &function, std::shared_ptr<RISCV::Stack> &stack);
}

class RISCV::RegisterAllocator::Allocator : public std::enable_shared_from_this<Allocator> {
    public:
        explicit Allocator(const std::shared_ptr<Backend::LIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack);
        virtual ~Allocator() = default;
        virtual void allocate() = 0;
        RISCV::Registers::ABI get_register(const std::shared_ptr<Backend::Variable>& variable) const;

        [[nodiscard]] std::string to_string() const;
    protected:
        std::shared_ptr<RISCV::Stack> stack;
        std::shared_ptr<Backend::LIR::Function> lir_function;
        std::unordered_map<std::string, RISCV::Registers::ABI> var_to_reg;
        static inline const std::array<RISCV::Registers::ABI, 15> caller_saved = {
            RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T1, RISCV::Registers::ABI::T2,
            RISCV::Registers::ABI::T3, RISCV::Registers::ABI::T4, RISCV::Registers::ABI::T5,
            RISCV::Registers::ABI::T6,
            RISCV::Registers::ABI::A0, RISCV::Registers::ABI::A1, RISCV::Registers::ABI::A2,
            RISCV::Registers::ABI::A3, RISCV::Registers::ABI::A4, RISCV::Registers::ABI::A5,
            RISCV::Registers::ABI::A6, RISCV::Registers::ABI::A7,
        };
        static inline const std::array<RISCV::Registers::ABI, 12> callee_saved = {
            RISCV::Registers::ABI::S0, RISCV::Registers::ABI::S1, RISCV::Registers::ABI::S2, RISCV::Registers::ABI::S3,
            RISCV::Registers::ABI::S4, RISCV::Registers::ABI::S5, RISCV::Registers::ABI::S6, RISCV::Registers::ABI::S7,
            RISCV::Registers::ABI::S8, RISCV::Registers::ABI::S9, RISCV::Registers::ABI::S10, RISCV::Registers::ABI::S11
        };
        std::vector<RISCV::Registers::ABI> available_integer_regs = {
            RISCV::Registers::ABI::A0, RISCV::Registers::ABI::A1, RISCV::Registers::ABI::A2, RISCV::Registers::ABI::A3,
            RISCV::Registers::ABI::A4, RISCV::Registers::ABI::A5, RISCV::Registers::ABI::A6, RISCV::Registers::ABI::A7,
            RISCV::Registers::ABI::S0, RISCV::Registers::ABI::S1, RISCV::Registers::ABI::S2, RISCV::Registers::ABI::S3,
            RISCV::Registers::ABI::S4, RISCV::Registers::ABI::S5, RISCV::Registers::ABI::S6, RISCV::Registers::ABI::S7,
            RISCV::Registers::ABI::S8, RISCV::Registers::ABI::S9, RISCV::Registers::ABI::S10, RISCV::Registers::ABI::S11,
            RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T1, RISCV::Registers::ABI::T2,
            RISCV::Registers::ABI::T3, RISCV::Registers::ABI::T4, RISCV::Registers::ABI::T5,
            RISCV::Registers::ABI::T6
        };
};

#endif
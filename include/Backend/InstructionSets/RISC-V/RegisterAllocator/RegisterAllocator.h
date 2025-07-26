#ifndef RV_REGISTER_ALLOCATOR_H
#define RV_REGISTER_ALLOCATOR_H

#include <memory>
#include <unordered_map>
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
};

#endif
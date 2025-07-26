#ifndef RV_REGISTER_ALLOCATOR_FGRAPH_COLORING_H
#define RV_REGISTER_ALLOCATOR_FGRAPH_COLORING_H

#include <algorithm>
#include <limits>
#include <set>
#include <stack>
#include <map>
#include <string>
#include <ostream>
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/GraphColoring.h"
#include "Backend/LIR/Instructions.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/LIR/LIR.h"
#include "Backend/VariableTypes.h"
#include "Utils/Log.h"

class RISCV::RegisterAllocator::FGraphColoring : public RISCV::RegisterAllocator::GraphColoring {
    public:
        explicit FGraphColoring(const std::shared_ptr<Backend::LIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack) : GraphColoring(function, stack) {
            is_consistent = Backend::Utils::is_float;
        }
        ~FGraphColoring() override = default;
        void allocate() final override;
    private:
        void create_registers() final override;
        void build_interference_graph() final override;
};

#endif
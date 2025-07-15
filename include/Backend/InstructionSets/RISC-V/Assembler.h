#ifndef RV_ASSEMBLER_H
#define RV_ASSEMBLER_H

#include <string>
#include <memory>
#include "Mir/Structure.h"
#include "Backend/LIR/LIR.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"

namespace RISCV {
    class Assembler
    {
        public:
            std::shared_ptr<Backend::LIR::Module> mir_module;
            std::shared_ptr<RISCV::Module> rv_module;
            RegisterAllocator::AllocationType allocation_type;

            explicit Assembler(const std::shared_ptr<Mir::Module> &llvm_module, RegisterAllocator::AllocationType type = RegisterAllocator::AllocationType::GRAPH_COLORING) : allocation_type(type) {
                mir_module = std::make_shared<Backend::LIR::Module>(llvm_module);
                mir_module->analyze_live_variables();
                // mir_module->print_live_variables();
                rv_module = std::make_shared<RISCV::Module>(mir_module, allocation_type);
                rv_module->to_assembly();
            }

            [[nodiscard]] std::string to_string() const;
    };
}
#endif
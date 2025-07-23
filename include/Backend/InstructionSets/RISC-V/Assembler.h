#ifndef RV_ASSEMBLER_H
#define RV_ASSEMBLER_H

#include <string>
#include <memory>
#include "Mir/Structure.h"
#include "Backend/LIR/LIR.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/Assembler.h"

namespace RISCV {
    class Assembler : public Backend::Assembler {
        public:
            RegisterAllocator::AllocationType allocation_type;

            explicit Assembler(const std::shared_ptr<Mir::Module> &llvm_module, RegisterAllocator::AllocationType type = RegisterAllocator::AllocationType::GRAPH_COLORING) : Backend::Assembler(llvm_module), allocation_type(type) {
                rv_module = std::make_shared<RISCV::Module>(lir_module, allocation_type);
                rv_module->to_assembly();
            }

            [[nodiscard]] std::string to_string() const override {
                return rv_module->to_string();
            }
        private:
            std::shared_ptr<RISCV::Module> rv_module;
    };
}
#endif
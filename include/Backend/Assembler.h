#ifndef BACKEND_ASSEMBLER_H
#define BACKEND_ASSEMBLER_H

#include <string>
#include "Backend/LIR/LIR.h"
#include "Backend/InstructionSets/RISC-V/Opt/Peephole.h"
#include "Backend/InstructionSets/RISC-V/Opt/Arithmetic.h"

namespace Backend {
    class Assembler {
        public:
            std::shared_ptr<Backend::LIR::Module> lir_module;
            [[nodiscard]] virtual std::string to_string() const = 0;

            Assembler(const std::shared_ptr<Mir::Module> &llvm_module) {
                lir_module = std::make_shared<Backend::LIR::Module>(llvm_module);
                auto arithmetic_opt = std::make_shared<RISCV::Opt::ConstOpt>(lir_module);
                arithmetic_opt->optimize();
                auto peephole_opt = std::make_shared<RISCV::Opt::PeepholeBeforeRA>(lir_module);
                peephole_opt->optimize();
            }
    };
}

#endif
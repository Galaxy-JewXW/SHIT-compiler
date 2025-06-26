#ifndef RV_ASSEMBLER_H
#define RV_ASSEMBLER_H

#include <string>
#include <memory>
#include "Mir/Structure.h"
#include "Backend/MIR/MIR.h"

namespace RISCV {
    // inline std::shared_ptr<RISCV::Modules::Function> CUR_FUNC = nullptr;
    const std::string RISCV_TEXT_SECTION = ".section .text\n.option norvc\n.global main\n";

    class Assembler
    {
        public:
            std::shared_ptr<Backend::MIR::Module> mir_module;

            explicit Assembler(const std::shared_ptr<Mir::Module> &llvm_module) {
                mir_module = std::make_shared<Backend::MIR::Module>(llvm_module);
            }

            [[nodiscard]] std::string to_string() const;

    };
}

#endif
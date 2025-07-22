#ifndef BACKEND_ASSEMBLER_H
#define BACKEND_ASSEMBLER_H

#include <string>
#include "Backend/LIR/LIR.h"

namespace Backend {
    class Assembler {
        public:
            std::shared_ptr<Backend::LIR::Module> lir_module;
            [[nodiscard]] virtual std::string to_string() const = 0;

            Assembler(const std::shared_ptr<Mir::Module> &llvm_module) {
                lir_module = std::make_shared<Backend::LIR::Module>(llvm_module);
            }
    };
}

#endif
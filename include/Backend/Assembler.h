#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "Mir/Structure.h"
#include "Backend/Instructions/RISC-V/Modules.h"
#include <map>

namespace Assembler {
    class RISCV_Assembler
    {
        public:
            explicit RISCV_Assembler() = default;
            explicit RISCV_Assembler(std::shared_ptr<Mir::Module> module);

            [[nodiscard]] std::string to_string() const;
        private:
            RISCV::Modules::TextField text;
            RISCV::Modules::DataField data;
            std::shared_ptr<Mir::Module> module;
    };
}
#endif
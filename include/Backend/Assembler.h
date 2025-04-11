#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "Mir/Structure.h"
#include "Backend/Instructions/RISC-V/Instruction.h"
#include <map>

namespace Assembler {
    class RISCV_Assembler
    {
        public:
            explicit RISCV_Assembler() = default;
            explicit RISCV_Assembler(std::shared_ptr<Mir::Module> module);

            [[nodiscard]] std::string to_string() const;
        private:
            RISCV_Instructions::TextField text;
            RISCV_Instructions::DataField data;
            std::shared_ptr<Mir::Module> module;
    };
}

#endif
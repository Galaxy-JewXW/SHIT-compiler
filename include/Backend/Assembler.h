#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "Mir/Structure.h"

namespace Backend::Assembler {
    class AssemblerBase {
        public:
            virtual ~AssemblerBase() = default;

            virtual void assemble(std::shared_ptr<Mir::Module> module) = 0;

            [[nodiscard]] virtual std::string to_string() const = 0;
    };
}
#endif
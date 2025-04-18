#ifndef RV_MEM_H
#define RV_MEM_H

#include "Mir/Instruction.h"
#include <string>
#include <map>

namespace RISCV::Memory {
    class Memory {
        public:
            std::map<std::string, size_t> vreg2offset;
    };
}

#endif
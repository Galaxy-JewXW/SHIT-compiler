#ifndef RV_OPT_PEEPHOLE_H
#define RV_OPT_PEEPHOLE_H

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "Backend/InstructionSets/RISC-V/Instructions.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Backend/LIR/DataSection.h"
#include "Backend/LIR/Instructions.h"
#include "Backend/LIR/LIR.h"
#include "Backend/Value.h"

namespace RISCV::Opt {
    class PeepholeBeforeRA;
    class PeepholeAfterRA;
}

class RISCV::Opt::PeepholeBeforeRA {
    public:
        PeepholeBeforeRA(const std::shared_ptr<Backend::LIR::Module> &module);
        ~PeepholeBeforeRA();
        std::shared_ptr<Backend::LIR::Module> module;
        void optimize();
        void addZero2Mv(const std::shared_ptr<Backend::LIR::Block> &block);
        void addiLS2LSOffset(const std::shared_ptr<Backend::LIR::Block> &block);
        void uselessLoadRemove(const std::shared_ptr<Backend::LIR::Block> &block);
        void preRAConstValueReUse(const std::shared_ptr<Backend::LIR::Block> &block);
        void preRAConstPointerReUse(const std::shared_ptr<Backend::LIR::Block>& block);

        /*-----------------------二轮----------------------*/
        void Lsw2Lsd(const std::shared_ptr<Backend::LIR::Block> &block);
};

class RISCV::Opt::PeepholeAfterRA {
    public:
        PeepholeAfterRA(const std::shared_ptr<RISCV::Module> &module);
        ~PeepholeAfterRA();
        std::shared_ptr<RISCV::Module> module;
        void optimize();
        void addSubZeroRemove(const std::shared_ptr<RISCV::Block> &block);
};

#endif

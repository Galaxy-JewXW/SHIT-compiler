#ifndef RV_REGISTER_ALLOCATOR_LINEAR_SCAN_H
#define RV_REGISTER_ALLOCATOR_LINEAR_SCAN_H

#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"

class RISCV::RegisterAllocator::LinearScan : public RISCV::RegisterAllocator::Allocator {
    public:
        explicit LinearScan(const std::shared_ptr<Backend::MIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack) : Allocator(function, stack) {}
        void allocate() override;
        RISCV::Registers::ABI use_register(const std::shared_ptr<Backend::MIR::Variable>& variable, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>>& instrs) override;
    private:
        struct LiveInterval {
            int start;
            int end;
            std::string var_name;
            bool operator<(const LiveInterval& other) const {
                return start < other.start;
            }
        };

        std::vector<LiveInterval> analyze_live_intervals(bool global);
        void allocate(std::vector<LiveInterval> &live_intervals, std::vector<RISCV::Registers::ABI> &available_regs);

        inline bool is_global(bool global, const std::shared_ptr<Backend::MIR::Variable>& var) {
            if (!var) return false;
            return (var->position != Backend::MIR::VariablePosition::LOCAL) ^ (!global);
        }
};

#endif

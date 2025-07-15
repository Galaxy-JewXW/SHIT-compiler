#ifndef RV_REGISTER_ALLOCATOR_LINEAR_SCAN_H
#define RV_REGISTER_ALLOCATOR_LINEAR_SCAN_H

#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/LIR/LIR.h"

class RISCV::RegisterAllocator::LinearScan : public RISCV::RegisterAllocator::Allocator {
    public:
        explicit LinearScan(const std::shared_ptr<Backend::LIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack) : Allocator(function, stack) {}
        void allocate() override;
        RISCV::Registers::ABI use_register_ro(const std::shared_ptr<Backend::Variable> &variable, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) override;
        RISCV::Registers::ABI use_register_w(const std::shared_ptr<Backend::Variable> &variable, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) override;
    private:
        struct LiveInterval {
            int start;
            int end;
            std::string var_name;
            std::shared_ptr<Backend::LIR::Instruction> end_instr;
            bool operator<(const LiveInterval& other) const {
                return start < other.start;
            }
        };

        std::vector<LiveInterval> analyze_live_intervals(bool global);
        std::vector<RISCV::Registers::ABI> unwritten_backed;
        std::unordered_map<std::string, std::shared_ptr<Backend::LIR::Instruction>> dead_at;
        std::unordered_map<RISCV::Registers::ABI, std::shared_ptr<Backend::Variable>> t_reg_to_var;
        void allocate(std::vector<LiveInterval> &live_intervals, std::vector<RISCV::Registers::ABI> &available_regs);

        inline bool is_global(bool global, const std::shared_ptr<Backend::Variable>& var) {
            if (!var) return false;
            return (var->lifetime != Backend::VariableWide::LOCAL) ^ (!global);
        }
};

#endif

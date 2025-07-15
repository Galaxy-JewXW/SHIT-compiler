#ifndef RV_REGISTER_ALLOCATOR_H
#define RV_REGISTER_ALLOCATOR_H

#include <memory>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <queue>
#include "Backend/LIR/LIR.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/InstructionSets/RISC-V/Instructions.h"

namespace RISCV::RegisterAllocator {
    class Allocator;
    class LinearScan;
    class GraphColoring;
    enum class AllocationType {
        LINEAR_SCAN,
        GRAPH_COLORING
    };
    std::shared_ptr<Allocator> create(AllocationType type, const std::shared_ptr<Backend::LIR::Function> &function, std::shared_ptr<RISCV::Stack> &stack);
}

namespace RISCV {
    class Stack;
}

class RISCV::RegisterAllocator::Allocator : public std::enable_shared_from_this<Allocator> {
    public:
        explicit Allocator(const std::shared_ptr<Backend::LIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack);
        virtual void allocate() = 0;
        RISCV::Registers::ABI get_register(const std::shared_ptr<Backend::Variable>& variable) const;
        virtual RISCV::Registers::ABI use_register_ro(const std::shared_ptr<Backend::Variable> &variable, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) = 0;
        virtual RISCV::Registers::ABI use_register_w(const std::shared_ptr<Backend::Variable> &variable, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) = 0;

        virtual void spill_caller_saved_registers(std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) = 0;
        virtual void restore_caller_saved_registers(std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) = 0;

        // 基本块边界处的寄存器状态管理
        virtual void prepare_for_new_block(const std::shared_ptr<Backend::LIR::Block> &block, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) = 0;
        virtual void finalize_block(const std::shared_ptr<Backend::LIR::Block> &block, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) = 0;

        virtual void spill_after_write(const std::shared_ptr<Backend::Variable> &variable, RISCV::Registers::ABI reg, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) = 0;
        virtual void cleanup_at_function_end(std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instructions) = 0;

        [[nodiscard]] inline std::string to_string() const {
            std::ostringstream oss;
            oss << "Register Allocator for function: " << mir_function->name << "\n";
            for (const auto& [var_name, var] : mir_function->variables) {
                if (var_to_reg.find(var_name) != var_to_reg.end()) {
                    oss << "  " << var_name << " -> " << RISCV::Registers::reg2string(var_to_reg.at(var_name)) << "\n";
                } else if (stack->stack_index.find(var_name) != stack->stack_index.end()) {
                    oss << "  " << var_name << " -> stack\n";
                } else {
                    log_error("Variable %s not found in register allocation map or stack index", var_name.c_str());
                }
            }
            return oss.str();
        }
    protected:
        std::shared_ptr<RISCV::Stack> stack;
        std::shared_ptr<Backend::LIR::Function> mir_function;
        std::unordered_map<std::string, RISCV::Registers::ABI> var_to_reg;
        std::vector<RISCV::Registers::ABI> available_temp_regs = {
            RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T1, RISCV::Registers::ABI::T2,
            RISCV::Registers::ABI::T3, RISCV::Registers::ABI::T4, RISCV::Registers::ABI::T5,
            RISCV::Registers::ABI::T6
        };
        std::vector<RISCV::Registers::ABI> available_global_regs = {
            RISCV::Registers::ABI::A0, RISCV::Registers::ABI::A1, RISCV::Registers::ABI::A2, RISCV::Registers::ABI::A3,
            RISCV::Registers::ABI::A4, RISCV::Registers::ABI::A5, RISCV::Registers::ABI::A6, RISCV::Registers::ABI::A7,
            RISCV::Registers::ABI::S0, RISCV::Registers::ABI::S1, RISCV::Registers::ABI::S2, RISCV::Registers::ABI::S3,
            RISCV::Registers::ABI::S4, RISCV::Registers::ABI::S5, RISCV::Registers::ABI::S6, RISCV::Registers::ABI::S7,
            RISCV::Registers::ABI::S8, RISCV::Registers::ABI::S9, RISCV::Registers::ABI::S10, RISCV::Registers::ABI::S11
        };
        std::vector<RISCV::Registers::ABI> available_integer_regs = {
            RISCV::Registers::ABI::A0, RISCV::Registers::ABI::A1, RISCV::Registers::ABI::A2, RISCV::Registers::ABI::A3,
            RISCV::Registers::ABI::A4, RISCV::Registers::ABI::A5, RISCV::Registers::ABI::A6, RISCV::Registers::ABI::A7,
            RISCV::Registers::ABI::S0, RISCV::Registers::ABI::S1, RISCV::Registers::ABI::S2, RISCV::Registers::ABI::S3,
            RISCV::Registers::ABI::S4, RISCV::Registers::ABI::S5, RISCV::Registers::ABI::S6, RISCV::Registers::ABI::S7,
            RISCV::Registers::ABI::S8, RISCV::Registers::ABI::S9, RISCV::Registers::ABI::S10, RISCV::Registers::ABI::S11,
            RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T1, RISCV::Registers::ABI::T2,
            RISCV::Registers::ABI::T3, RISCV::Registers::ABI::T4, RISCV::Registers::ABI::T5,
            RISCV::Registers::ABI::T6
        };
};

namespace RISCV::Utils {
    void analyze_live_variables(std::shared_ptr<Backend::LIR::Function> &function);
    bool analyze_live_variables(std::shared_ptr<Backend::LIR::Block> &block, std::vector<std::string> &visited);
}

#endif
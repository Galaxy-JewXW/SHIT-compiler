#ifndef RV_MODULES_H
#define RV_MODULES_H

#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include <algorithm>
#include "Backend/LIR/LIR.h"
#include "Backend/InstructionSets/RISC-V/Instructions.h"
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/VariableTypes.h"
#include "Backend/InstructionSets/RISC-V/memset.h"
#include "Backend/Value.h"

namespace RISCV {
    class Module;
    class Function;
    class Block;
    class Stack;
}

class RISCV::Stack {
    public:
        std::unordered_map<std::string, int32_t> stack_index;
        static inline const uint32_t RA_SIZE = 8 * __BYTE__;
        uint32_t stack_size{RA_SIZE};

        inline void add_variable(const std::shared_ptr<Backend::Variable> &variable) {
            if (stack_index.find(variable->name) != stack_index.end())
                return;
            if (variable->lifetime != Backend::VariableWide::FUNCTIONAL)
                log_error("`%s` should not be stored in stack", variable->name.c_str());
            stack_size += variable->size();
            stack_index[variable->name] = stack_size;
        }

        inline void add_parameter(const std::shared_ptr<Backend::Variable> &variable, const int32_t sp_plus) {
            if (variable->lifetime != Backend::VariableWide::FUNCTIONAL)
                log_error("`%s` should not be stored in stack", variable->name.c_str());
            stack_index[variable->name] = -sp_plus;
        }

        int align(const size_t alignment) {
            return (stack_size + alignment - 1) & ~(alignment - 1);
        }

        int32_t offset_of(const std::shared_ptr<Backend::Variable> &variable) {
            if (variable->lifetime == Backend::VariableWide::FUNCTIONAL) {
                if (stack_index.find(variable->name) == stack_index.end())
                    log_error("`%s` is not stored in stack", variable->name.c_str());
                int32_t position = stack_index[variable->name];
                if (position < 0)
                    return position;
                return align(16) - position;
            } log_error("`%s` should not be stored in stack", variable->name.c_str());
        }
};

class RISCV::Block {
    public:
        std::string name;
        std::vector<std::shared_ptr<Instructions::Instruction>> instructions;
        std::weak_ptr<RISCV::Function> function;

        explicit Block(std::string name, std::shared_ptr<RISCV::Function> function) : name(name), function(function) {}
        [[nodiscard]] std::string to_string() const;
        [[nodiscard]] std::string label_name() const;
};

class RISCV::Function : public std::enable_shared_from_this<RISCV::Function> {
    public:
        std::string name;
        std::vector<std::shared_ptr<RISCV::Block>> blocks;
        std::shared_ptr<RISCV::RegisterAllocator::Allocator> register_allocator;
        std::shared_ptr<Stack> stack;
        RISCV::Module *module;

        explicit Function(const std::shared_ptr<Backend::LIR::Function>& lir_function, const RegisterAllocator::AllocationType& allocation_type = RegisterAllocator::AllocationType::LINEAR_SCAN);

        void to_assembly() {
            translate_blocks();
            generate_prologue();
        }

        [[nodiscard]] std::string to_string() const;

    private:
        std::shared_ptr<Backend::LIR::Function> lir_function;
        void generate_prologue();
        void translate_blocks();
        inline std::shared_ptr<RISCV::Block> find_block(std::string name) const {
            for (std::shared_ptr<RISCV::Block> block: blocks)
                if (block->name == name)
                    return block;
            return nullptr;
        }
        [[nodiscard]] std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> translate_instruction(const std::shared_ptr<Backend::LIR::Instruction>& instruction);

        template<typename T_imm, typename T_reg>
        void translate_iactions(const std::shared_ptr<Backend::LIR::IntArithmetic> &instr, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instrs);
        template<typename T_instr>
        void translate_bactions(const std::shared_ptr<Backend::LIR::IBranch> &instr, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instrs);
};

class RISCV::Module {
    public:
        std::vector<std::shared_ptr<Function>> functions;
        std::shared_ptr<Backend::DataSection> data_section;

        explicit Module(const std::shared_ptr<Backend::LIR::Module>& lir_module, const RegisterAllocator::AllocationType& allocation_type = RegisterAllocator::AllocationType::LINEAR_SCAN);
        [[nodiscard]] std::string to_string() const;

        inline void to_assembly() {
            for (std::shared_ptr<RISCV::Function>& function : functions)
                function->to_assembly();
        }
    private:
        static std::string to_string(const std::shared_ptr<Backend::DataSection> &data_section);
        static inline const std::string TEXT_OPTION =
"\
.section .text\n\
.option norvc\n\
.global main\n\
";
};

#endif
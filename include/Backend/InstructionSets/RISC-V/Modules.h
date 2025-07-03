#ifndef RV_MODULES_H
#define RV_MODULES_H

#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include <algorithm>
#include "Backend/MIR/MIR.h"
#include "Backend/InstructionSets/RISC-V/Instructions.h"
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/VariableTypes.h"

namespace RISCV {
    class Module;
    class Function;
    class Block;
    class Stack;
}

class RISCV::Stack {
    public:
        std::unordered_map<std::string, int64_t> stack_index;
        std::vector<std::shared_ptr<Backend::MIR::Variable>> stack;
        static inline const uint64_t SP_SIZE = 8 * __BYTE__;
        static inline const uint64_t RA_SIZE = 8 * __BYTE__;
        uint64_t stack_size{SP_SIZE + RA_SIZE};

        inline void add_variable(const std::shared_ptr<Backend::MIR::Variable> &variable) {
            stack_size += Backend::Utils::type_to_size(variable->type);
            stack_index[variable->name] = stack_size;
            stack.push_back(variable);
        }
};

class RISCV::Block {
    public:
        std::string name;
        std::vector<std::shared_ptr<Instructions::Instruction>> instructions;
        std::weak_ptr<RISCV::Function> function;

        explicit Block(std::string name, std::shared_ptr<RISCV::Function> function) : name(name), function(function) {}
        [[nodiscard]] std::string to_string() const;
};

class RISCV::Function : public std::enable_shared_from_this<RISCV::Function> {
    public:
        std::string name;
        std::vector<std::shared_ptr<RISCV::Block>> blocks;
        std::shared_ptr<RISCV::RegisterAllocator::Allocator> register_allocator;
        std::shared_ptr<Stack> stack;

        explicit Function(const std::shared_ptr<Backend::MIR::Function>& mir_function, const RegisterAllocator::AllocationType& allocation_type = RegisterAllocator::AllocationType::LINEAR_SCAN);

        void to_assembly();
        [[nodiscard]] std::string to_string() const;

    private:
        std::shared_ptr<Backend::MIR::Function> mir_function_;
        void generate_prologue();
        void translate_blocks();
        inline std::shared_ptr<RISCV::Block> find_block(std::string name) const {
            for (std::shared_ptr<RISCV::Block> block: blocks)
                if (block->name == name)
                    return block;
            return nullptr;
        }
        [[nodiscard]] std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> translate_instruction(const std::shared_ptr<Backend::MIR::Instruction>& instruction);
};

class RISCV::Module {
    public:
        std::vector<std::shared_ptr<Function>> functions;
        std::shared_ptr<Backend::DataSection> data_section;

        explicit Module(const std::shared_ptr<Backend::MIR::Module>& mir_module, const RegisterAllocator::AllocationType& allocation_type = RegisterAllocator::AllocationType::LINEAR_SCAN);
        [[nodiscard]] std::string to_string() const;
    private:
        static inline const std::string TEXT_OPTION =
"\
.section .text\n\
.option norvc\n\
.global main\n\
";
};

#endif
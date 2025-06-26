#ifndef BACKEND_MIR_H
#define BACKEND_MIR_H

#include <memory>
#include <string>
#include <map>
#include <vector>
#include "Backend/MIR/DataSection.h"
#include "Backend/VariableTypes.h"
#include "Mir/Structure.h"
#include "Mir/Instruction.h"
#include "Utils/Log.h"

namespace Backend::MIR {
    class Module;
    class Function;
    class Block;
    class Instruction;
    class Value;
    class Constant;
    class Variable;
    class Register;
    enum class OperandType : uint32_t {
        CONSTANT,
        VARIABLE
    };
    enum class InstructionType : uint32_t {
        ADD,
        FADD,
        SUB,
        FSUB,
        MUL,
        FMUL,
        DIV,
        FDIV,
        MOD,
        LOAD,
        STORE,
        CALL,
        RETURN,
        JUMP,
        BRANCH_ON_ZERO,
        BRANCH_ON_NON_ZERO,
        BRANCH_ON_EQUAL,
        BRANCH_ON_NOT_EQUAL,
        BRANCH_ON_GREATER_THAN,
        BRANCH_ON_LESS_THAN,
        BRANCH_ON_GREATER_THAN_OR_EQUAL,
        BRANCH_ON_LESS_THAN_OR_EQUAL,
        BITWISE_AND,
        BITWISE_OR,
        BITWISE_XOR,
        BITWISE_NOT,
        SHIFT_LEFT,
        SHIFT_RIGHT,
        SHIFT_LEFT_LOGICAL,
        SHIFT_RIGHT_LOGICAL,
        SHIFT_RIGHT_ARITHMETIC,
        PUTF,
        LOAD_ADDR,
    };
    [[nodiscard]] std::string InstructionType_to_string(InstructionType type);
    class ArithmeticInstruction;
    class CallInstruction;
    class MemoryInstruction;
    class BranchInstruction;
    class ReturnInstruction;
    class FunctionVariable;
    class LocalVariable;
    enum VariablePosition {
        GLOBAL,
        FUNCTION,
        LOCAL,
    };
};

class Backend::MIR::Value {
    public:
        std::string name;
        OperandType value_type;

        virtual ~Value() = default;
        Value(const std::string &name) : name(name), value_type(OperandType::VARIABLE) {};
        virtual std::string to_string() const {
            return name;
        }
};

class Backend::MIR::Constant : public Backend::MIR::Value {
    public:
        explicit Constant(const int64_t value) : Backend::MIR::Value(std::to_string(value)) {
            this->value_type = OperandType::CONSTANT;
        };
};

class Backend::MIR::Variable : public Backend::MIR::Value {
    public:
        Backend::VariableType type{Backend::VariableType::INT32};
        VariablePosition position{VariablePosition::GLOBAL};
        std::shared_ptr<Backend::MIR::Instruction> defined_in;
        std::vector<std::shared_ptr<Backend::MIR::Instruction>> used_in;

        explicit Variable(const std::string &name) : Backend::MIR::Value(name) { this->value_type = OperandType::VARIABLE; };

        explicit Variable(const std::string &name, const Backend::VariableType &type) : Backend::MIR::Value(name), type(type) { this->value_type = OperandType::VARIABLE; }
};

class Backend::MIR::FunctionVariable : public Backend::MIR::Variable {
    public:
        explicit FunctionVariable() = default;
        explicit FunctionVariable(const std::string &name, const Backend::VariableType &type): Backend::MIR::Variable(name, type) {
            this->position = VariablePosition::FUNCTION;
        }
        explicit FunctionVariable(const Backend::DataSection::Variable &var) : Backend::MIR::Variable(var.name, var.type) {}
};

class Backend::MIR::LocalVariable : public Backend::MIR::Variable {
    public:
        explicit LocalVariable() = default;
        explicit LocalVariable(const std::string &name, const Backend::VariableType &type): Backend::MIR::Variable(name, type) {
            this->position = VariablePosition::LOCAL;
        }
};

class Backend::MIR::Instruction {
    public:
        InstructionType type;
        std::weak_ptr<Backend::MIR::Block> parent_block;
        virtual ~Instruction() = default;
        Instruction(InstructionType type) : type(type) {}
        virtual std::string to_string() const = 0;
};

class Backend::MIR::Block {
    public:
        std::string name;
        std::vector<std::shared_ptr<Backend::MIR::Instruction>> instructions;
        std::vector<std::weak_ptr<Backend::MIR::Block>> predecessors;
        std::vector<std::weak_ptr<Backend::MIR::Block>> successors;
        std::weak_ptr<Backend::MIR::Function> parent_function;

        explicit Block(const std::string &block_name) : name(std::move(block_name)) {};
        [[nodiscard]] inline std::string to_string() const {
            std::ostringstream oss;
            oss << " " << name << ":\n";
            for (const std::shared_ptr<Backend::MIR::Instruction> &instr : instructions) {
                oss << "  " << instr->to_string() << "\n";
            }
            return oss.str();
        }
};

class Backend::MIR::Function {
    public:
        std::string name;
        std::map<std::string, std::shared_ptr<Backend::MIR::Block>> blocks;
        Backend::VariableType return_type{Backend::VariableType::INT32};
        std::map<std::string, std::shared_ptr<Backend::MIR::Variable>> variables;

        explicit Function(const std::string &function_name) : name(std::move(function_name)) {};

        [[nodiscard]] inline std::string to_string() const {
            std::ostringstream oss;
            oss << this->name << ":\n";
            for (const std::pair<const std::string, std::shared_ptr<Backend::MIR::Block>> &block : blocks) {
                oss << block.second->to_string() << "\n";
            }
            return oss.str();
        }

        inline void add_variable(const std::shared_ptr<Backend::MIR::Variable> variable) {
            variables[variable->name] = variable;
        }
};

class Backend::MIR::Module : public std::enable_shared_from_this<Backend::MIR::Module> {
    public:
        std::shared_ptr<Mir::Module> llvm_module;
        std::map<std::string, std::shared_ptr<Backend::MIR::Function>> functions;
        std::shared_ptr<Backend::DataSection> global_data;

        explicit Module(const std::shared_ptr<Mir::Module> &llvm_module) : llvm_module(llvm_module) {
            load_global_data(llvm_module);
            load_functions_and_blocks(llvm_module);
            for (const std::shared_ptr<Mir::Function> &llvm_function : llvm_module->get_functions()) {
                std::shared_ptr<Backend::MIR::Function> function = functions[llvm_function->get_name()];
                load_instructions(llvm_function, function);
            }
        }

        [[nodiscard]] inline std::string to_string() const {
            std::ostringstream oss;
            oss << global_data->to_string() << "\n";
            for (const std::pair<const std::string, std::shared_ptr<Backend::MIR::Function>> &pair : functions) {
                oss << pair.second->to_string() << "\n";
            }
            return oss.str();
        }
    private:
        std::shared_ptr<Backend::MIR::Variable> find_variable(const std::string &name, const std::shared_ptr<Backend::MIR::Function> &function);
        std::shared_ptr<Backend::MIR::Value> find_value(const std::shared_ptr<Mir::Value> &value, const std::shared_ptr<Backend::MIR::Function> &function);
        void load_global_data(const std::shared_ptr<Mir::Module> &llvm_module);
        void load_functions_and_blocks(const std::shared_ptr<Mir::Module> &llvm_module);
        void load_instructions(const std::shared_ptr<Mir::Function> &llvm_function, std::shared_ptr<Backend::MIR::Function> &mir_function);
        void load_instruction(const std::shared_ptr<Mir::Instruction> &llvm_instruction, std::shared_ptr<Backend::MIR::Block> &mir_block);
};

#endif
#ifndef BACKEND_MIR_H
#define BACKEND_MIR_H

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include "Backend/MIR/DataSection.h"
#include "Backend/VariableTypes.h"
#include "Mir/Structure.h"
#include "Mir/Instruction.h"
#include "Utils/Log.h"

namespace Backend::MIR {
    class Module;
    class Function;
    class PrivilegedFunction;
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
    enum class FunctionType : uint32_t {
        NORMAL,
        PRIVILEGED
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
        MOVE,
        PHI,
    };
    class ArithmeticInstruction;
    class CallInstruction;
    class MemoryInstruction;
    class BranchInstruction;
    class ReturnInstruction;
    class PhiInstruction;
    class FunctionVariable;
    class LocalVariable;
    class ElementPointer;
    class CompareVariable;
    class Parameter;
    enum VariablePosition {
        GLOBAL,
        PARAMETER,
        FUNCTION,
        LOCAL,
        ELEMENT_POINTER,
        COMPARE,
    };
    const std::string privileged_function_names[] = {
        "getint", "putint", "getfloat", "putchar", "getchar", "exit"
    };
};

namespace Backend::Utils {
    [[nodiscard]] Backend::MIR::InstructionType llvm_to_mir(const Mir::IntBinary::Op &op);
    [[nodiscard]] Backend::MIR::InstructionType llvm_to_mir(const Mir::FloatBinary::Op &op);
    [[nodiscard]] Backend::MIR::InstructionType llvm_to_mir(const Mir::Icmp::Op &op);
    [[nodiscard]] std::string to_string(const Backend::MIR::InstructionType &type);
}

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
        int32_t int32_value{0};
        explicit Constant(const int32_t value) : Backend::MIR::Value(std::to_string(value)), int32_value(value) {
            this->value_type = OperandType::CONSTANT;
        };
};

class Backend::MIR::Variable : public Backend::MIR::Value {
    public:
        Backend::VariableType type{Backend::VariableType::INT32};
        VariablePosition position{VariablePosition::GLOBAL};

        explicit Variable(const std::string &name) : Backend::MIR::Value(name) { this->value_type = OperandType::VARIABLE; };

        explicit Variable(const std::string &name, const Backend::VariableType &type) : Backend::MIR::Value(name), type(type) { this->value_type = OperandType::VARIABLE; }
};

class Backend::MIR::Parameter : public Backend::MIR::Variable {
    public:
        explicit Parameter(const std::string &name, const Backend::VariableType &type) : Backend::MIR::Variable(name, type) {
            this->position = VariablePosition::PARAMETER;
        }
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

class Backend::MIR::ElementPointer : public Backend::MIR::Variable {
    public:
        std::shared_ptr<Backend::MIR::Variable> base_variable;
        std::shared_ptr<Backend::MIR::Value> offset;

        explicit ElementPointer(const std::string &name, const Backend::VariableType &type, const std::shared_ptr<Backend::MIR::Variable> &base_variable, const std::shared_ptr<Backend::MIR::Value> &offset)
            : Backend::MIR::Variable(name, type), base_variable(base_variable), offset(offset) {
            this->position = VariablePosition::ELEMENT_POINTER;
        }
};

class Backend::MIR::CompareVariable : public Backend::MIR::Variable {
    public:
        std::shared_ptr<Backend::MIR::Value> lhs;
        std::shared_ptr<Backend::MIR::Value> rhs;
        Backend::MIR::InstructionType compare_type;

        explicit CompareVariable(const std::string &name, const Backend::VariableType &type, const std::shared_ptr<Backend::MIR::Value> &lhs, const std::shared_ptr<Backend::MIR::Value> &rhs, Backend::MIR::InstructionType compare_type)
            : Backend::MIR::Variable(name, type), lhs(lhs), rhs(rhs), compare_type(compare_type) {
            this->position = VariablePosition::COMPARE;
        }

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << lhs->to_string() << " " << Backend::Utils::to_string(compare_type) << " " << rhs->to_string();
            return oss.str();
        }
};

class Backend::MIR::Instruction {
    public:
        InstructionType type;
        std::weak_ptr<Backend::MIR::Block> parent_block;

        virtual ~Instruction() = default;
        Instruction(InstructionType type) : type(type) {}

        virtual std::shared_ptr<Backend::MIR::Variable> get_defined_variable() const = 0;
        virtual std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> get_used_variables() const = 0;

        virtual std::string to_string() const = 0;
};

class Backend::MIR::Block {
    public:
        std::string name;
        std::vector<std::shared_ptr<Backend::MIR::Instruction>> instructions;
        std::vector<std::shared_ptr<Backend::MIR::Block>> predecessors;
        std::vector<std::shared_ptr<Backend::MIR::Block>> successors;
        std::weak_ptr<Backend::MIR::Function> parent_function;

        std::unordered_set<std::shared_ptr<Backend::MIR::Variable>> live_in;
        std::unordered_set<std::shared_ptr<Backend::MIR::Variable>> live_out;

        explicit Block(const std::string &block_name) : name(std::move(block_name)) {};

        [[nodiscard]] inline std::string to_string() const {
            std::ostringstream oss;
            oss << " " << name << ":\n";
            for (const std::shared_ptr<Backend::MIR::Instruction> &instr : instructions) {
                oss << "  " << instr->to_string() << "\n";
            }
            return oss.str();
        }

        [[nodiscard]] bool analyze_live_variables(std::shared_ptr<std::vector<std::string>> visited);
};

class Backend::MIR::Function {
    public:
        std::string name;
        std::map<std::string, std::shared_ptr<Backend::MIR::Block>> blocks_index;
        std::vector<std::shared_ptr<Backend::MIR::Block>> blocks;
        Backend::VariableType return_type{Backend::VariableType::INT32};
        std::map<std::string, std::shared_ptr<Backend::MIR::Variable>> variables;
        std::vector<std::shared_ptr<Backend::MIR::Variable>> parameters;
        FunctionType function_type{FunctionType::NORMAL};

        explicit Function(const std::string &function_name) : name(std::move(function_name)) {};
        virtual ~Function() = default;

        [[nodiscard]] inline std::string to_string() const {
            std::ostringstream oss;
            oss << this->name << ":\n";
            for (const std::shared_ptr<Backend::MIR::Block> &block : blocks) {
                oss << block->to_string() << "\n";
            }
            return oss.str();
        }

        inline void add_variable(const std::shared_ptr<Backend::MIR::Variable> variable) {
            variables[variable->name] = variable;
        }

        inline void add_block(const std::shared_ptr<Backend::MIR::Block> &block) {
            blocks.push_back(block);
            blocks_index[block->name] = block;
        }

        void analyze_live_variables();
};

class Backend::MIR::PrivilegedFunction : public Backend::MIR::Function {
    public:
        explicit PrivilegedFunction(const std::string &function_name) : Function(std::move(function_name)) {
            this->function_type = FunctionType::PRIVILEGED;
        };
};

class Backend::MIR::Module : public std::enable_shared_from_this<Backend::MIR::Module> {
    public:
        std::shared_ptr<Mir::Module> llvm_module;
        std::unordered_map<std::string, std::shared_ptr<Backend::MIR::Function>> functions_index;
        std::vector<std::shared_ptr<Backend::MIR::Function>> functions;
        std::shared_ptr<Backend::DataSection> global_data;

        explicit Module(const std::shared_ptr<Mir::Module> &llvm_module) : llvm_module(llvm_module) {
            load_global_data(llvm_module);
            load_functions_and_blocks(llvm_module);
            for (const std::shared_ptr<Mir::Function> &llvm_function : llvm_module->get_functions()) {
                std::shared_ptr<Backend::MIR::Function> function = functions_index[llvm_function->get_name()];
                load_instructions(llvm_function, function);
            }
        }

        [[nodiscard]] inline std::string to_string() const {
            std::ostringstream oss;
            oss << global_data->to_string() << "\n";
            for (const std::shared_ptr<Backend::MIR::Function> &function : functions) {
                oss << function->to_string() << "\n";
            }
            return oss.str();
        }

        inline void add_function(const std::shared_ptr<Backend::MIR::Function> &function) {
            functions.push_back(function);
            functions_index[function->name] = function;
        }

        std::shared_ptr<Backend::MIR::Variable> find_variable(const std::string &name, const std::shared_ptr<Backend::MIR::Function> &function);
        std::shared_ptr<Backend::MIR::Value> find_value(const std::shared_ptr<Mir::Value> &value, const std::shared_ptr<Backend::MIR::Function> &function);
        void load_functions_and_blocks(const std::shared_ptr<Mir::Module> &module);
        void load_instructions(const std::shared_ptr<Mir::Function> &function, std::shared_ptr<Backend::MIR::Function> &mir_function);
        void load_global_data(const std::shared_ptr<Mir::Module> &module);
        void analyze_live_variables();

        void print_live_variables() const;
        inline std::shared_ptr<Backend::DataSection> get_data_section() const { return global_data; }
    private:
        void load_instruction(const std::shared_ptr<Mir::Instruction> &instruction, std::shared_ptr<Backend::MIR::Block> &block);
};

#endif
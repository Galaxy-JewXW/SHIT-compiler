#ifndef BACKEND_MIR_H
#define BACKEND_MIR_H

#include <array>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <sstream>
#include "Backend/LIR/DataSection.h"
#include "Backend/VariableTypes.h"
#include "Backend/Value.h"
#include "Mir/Structure.h"
#include "Mir/Instruction.h"
#include "Utils/Log.h"

namespace Backend::LIR {
    class Module;
    class Function;
    class PrivilegedFunction;
    class Block;
    class Instruction;
    enum class FunctionType : uint32_t { UNPRIVILEGED, PRIVILEGED };
};

namespace Backend::LIR {
    enum class InstructionType : uint32_t {
        ADD, FADD,
        SUB, FSUB,
        MUL, FMUL,
        DIV, FDIV,
        MOD, FMOD,
        LOAD, LOAD_IMM, LOAD_ADDR, FLOAD,
        STORE, FSTORE,
        CALL,
        RETURN,
        JUMP,
        EQUAL, NOT_EQUAL, GREATER, GREATER_EQUAL, LESS, LESS_EQUAL,
        EQUAL_ZERO, NOT_EQUAL_ZERO, GREATER_ZERO, GREATER_EQUAL_ZERO, LESS_ZERO, LESS_EQUAL_ZERO,
        BITWISE_AND, BITWISE_OR, BITWISE_XOR, BITWISE_NOT,
        SHIFT_LEFT,
        SHIFT_RIGHT,
        SHIFT_LEFT_LOGICAL,
        SHIFT_RIGHT_LOGICAL,
        SHIFT_RIGHT_ARITHMETIC,
        PUTF,
        MOVE,
    };
    class IntArithmetic;
    class FloatArithmetic;
    class LoadI32;
    class LoadFloat;
    class Call;
    class LoadAddress;
    class LoadInt;
    class LoadFloat;
    class StoreInt;
    class StoreFloat;
    class BranchInstruction;
    class JumpInstruction;
    class ReturnInstruction;
    class Move;
};

namespace Backend::Utils {
    [[nodiscard]] Backend::LIR::InstructionType llvm_to_mir(const Mir::IntBinary::Op &op)  {
        switch (op) {
            case Mir::IntBinary::Op::ADD: return Backend::LIR::InstructionType::ADD;
            case Mir::IntBinary::Op::SUB: return Backend::LIR::InstructionType::SUB;
            case Mir::IntBinary::Op::MUL: return Backend::LIR::InstructionType::MUL;
            case Mir::IntBinary::Op::DIV: return Backend::LIR::InstructionType::DIV;
            case Mir::IntBinary::Op::MOD: return Backend::LIR::InstructionType::MOD;
            default: throw std::invalid_argument("Invalid operation");
        }
    }

    [[nodiscard]] Backend::LIR::InstructionType llvm_to_mir(const Mir::FloatBinary::Op &op)  {
        switch (op) {
            case Mir::FloatBinary::Op::ADD: return Backend::LIR::InstructionType::FADD;
            case Mir::FloatBinary::Op::SUB: return Backend::LIR::InstructionType::FSUB;
            case Mir::FloatBinary::Op::MUL: return Backend::LIR::InstructionType::FMUL;
            case Mir::FloatBinary::Op::DIV: return Backend::LIR::InstructionType::FDIV;
            case Mir::FloatBinary::Op::MOD: return Backend::LIR::InstructionType::FMOD;
            default: throw std::invalid_argument("Invalid operation");
        }
    }

    [[nodiscard]] std::string to_string(const Backend::LIR::InstructionType type) {
        switch (type) {
            case Backend::LIR::InstructionType::ADD:
            case Backend::LIR::InstructionType::FADD: return "+";
            case Backend::LIR::InstructionType::SUB:
            case Backend::LIR::InstructionType::FSUB: return "-";
            case Backend::LIR::InstructionType::MUL:
            case Backend::LIR::InstructionType::FMUL: return "*";
            case Backend::LIR::InstructionType::DIV:
            case Backend::LIR::InstructionType::FDIV: return "/";
            case Backend::LIR::InstructionType::MOD:
            case Backend::LIR::InstructionType::FMOD: return "%";
            case Backend::LIR::InstructionType::LOAD: return "load";
            case Backend::LIR::InstructionType::STORE: return "store";
            case Backend::LIR::InstructionType::CALL: return "call";
            case Backend::LIR::InstructionType::RETURN: return "return";
            case Backend::LIR::InstructionType::JUMP: return "jump";
            case Backend::LIR::InstructionType::EQUAL: return "==";
            case Backend::LIR::InstructionType::EQUAL_ZERO: return "== 0";
            case Backend::LIR::InstructionType::NOT_EQUAL: return "!=";
            case Backend::LIR::InstructionType::NOT_EQUAL_ZERO: return "!= 0";
            case Backend::LIR::InstructionType::GREATER: return ">";
            case Backend::LIR::InstructionType::GREATER_ZERO: return "> 0";
            case Backend::LIR::InstructionType::GREATER_EQUAL: return ">=";
            case Backend::LIR::InstructionType::GREATER_EQUAL_ZERO: return ">= 0";
            case Backend::LIR::InstructionType::LESS: return "<";
            case Backend::LIR::InstructionType::LESS_ZERO: return "< 0";
            case Backend::LIR::InstructionType::LESS_EQUAL: return "<=";
            case Backend::LIR::InstructionType::LESS_EQUAL_ZERO: return "<= 0";
            case Backend::LIR::InstructionType::BITWISE_AND: return "&";
            case Backend::LIR::InstructionType::BITWISE_OR: return "|";
            case Backend::LIR::InstructionType::BITWISE_XOR: return "^";
            case Backend::LIR::InstructionType::BITWISE_NOT: return "!";
            case Backend::LIR::InstructionType::SHIFT_LEFT: return "<<";
            case Backend::LIR::InstructionType::SHIFT_RIGHT: return ">>";
            case Backend::LIR::InstructionType::SHIFT_LEFT_LOGICAL: return "SHIFT_LEFT_LOGICAL";
            case Backend::LIR::InstructionType::SHIFT_RIGHT_LOGICAL: return "SHIFT_RIGHT_LOGICAL";
            case Backend::LIR::InstructionType::SHIFT_RIGHT_ARITHMETIC: return "SHIFT_RIGHT_ARITHMETIC";
            case Backend::LIR::InstructionType::PUTF: return "putf";
            case Backend::LIR::InstructionType::LOAD_ADDR: return "&";
            default: return "";
        }
    }

    template <typename T>
    [[nodiscard]] T compute(const Backend::LIR::InstructionType &op, T lhs, T rhs) {
        switch (op) {
            case Backend::LIR::InstructionType::ADD: return lhs + rhs;
            case Backend::LIR::InstructionType::SUB: return lhs - rhs;
            case Backend::LIR::InstructionType::MUL: return lhs * rhs;
            case Backend::LIR::InstructionType::DIV: return lhs / rhs;
            case Backend::LIR::InstructionType::MOD: return lhs % rhs;
            case Backend::LIR::InstructionType::FADD: return lhs + rhs;
            case Backend::LIR::InstructionType::FSUB: return lhs - rhs;
            case Backend::LIR::InstructionType::FMUL: return lhs * rhs;
            case Backend::LIR::InstructionType::FDIV: return lhs / rhs;
            default: throw std::invalid_argument("Invalid operation");
        }
    }

    [[nodiscard]] std::string unique_name(const std::string &prefix = "") {
        static size_t counter = 0;
        return prefix + std::to_string(counter);
    }
};

class Backend::LIR::Instruction {
    public:
        InstructionType type;
        std::weak_ptr<Backend::LIR::Block> parent_block;

        virtual ~Instruction() = default;
        Instruction(InstructionType type) : type(type) {}

        virtual std::shared_ptr<Backend::Variable> get_defined_variable() const { return nullptr; }
        virtual std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const { return {}; }

        virtual std::string to_string() const = 0;
};

class Backend::LIR::Block {
    public:
        std::string name;
        std::vector<std::shared_ptr<Backend::LIR::Instruction>> instructions;
        std::vector<std::shared_ptr<Backend::LIR::Block>> predecessors;
        std::vector<std::shared_ptr<Backend::LIR::Block>> successors;
        std::weak_ptr<Backend::LIR::Function> parent_function;

        std::unordered_set<std::shared_ptr<Backend::Variable>> live_in;
        std::unordered_set<std::shared_ptr<Backend::Variable>> live_out;

        explicit Block(const std::string &block_name) : name(std::move(block_name)) {};
};

class Backend::LIR::Function {
    public:
        std::string name;
        std::map<std::string, std::shared_ptr<Backend::LIR::Block>> blocks_index;
        std::vector<std::shared_ptr<Backend::LIR::Block>> blocks;
        Backend::VariableType return_type{Backend::VariableType::INT32};
        std::map<std::string, std::shared_ptr<Backend::Variable>> variables;
        std::vector<std::shared_ptr<Backend::Variable>> parameters;
        FunctionType function_type{FunctionType::UNPRIVILEGED};

        explicit Function(const std::string &function_name) : name(std::move(function_name)) {};
        Function(const std::string &function_name, std::vector<std::shared_ptr<Backend::Variable>> &&params)
            : name(std::move(function_name)), parameters(std::move(params)) {}

        virtual ~Function() = default;

        inline void add_variable(const std::shared_ptr<Backend::Variable> &variable) {
            variables[variable->name] = variable;
        }

        inline void remove_variable(const std::shared_ptr<Backend::Variable> &variable) {
            variables.erase(variable->name);
        }

        [[nodiscard]] inline std::shared_ptr<Backend::Variable> find_variable(const std::string &name) const {
            std::map<std::string, std::shared_ptr<Backend::Variable>>::const_iterator it = variables.find(name);
            if (it != variables.end()) {
                return it->second;
            }
            return nullptr;
        }

        inline void add_block(const std::shared_ptr<Backend::LIR::Block> &block) {
            blocks.push_back(block);
            blocks_index[block->name] = block;
        }

        inline void spill(std::shared_ptr<Backend::Variable> &local_variable) {
            static int counter = 0;
            if (local_variable->lifetime != VariableWide::LOCAL)
                log_error("Only variable in register can be spilled.");
            local_variable->lifetime = VariableWide::FUNCTIONAL;
            for (std::shared_ptr<Backend::LIR::Block> &block : blocks) {
                for (std::shared_ptr<Backend::LIR::Instruction> &instr : block->instructions) {
                    if (instr->get_defined_variable() == local_variable) {
                        // insert `store` after the instruction
                        // block->instructions.insert(block->instructions.begin() + (std::find(block->instructions.begin(), block->instructions.end(), instr) - block->instructions.begin()) + 1, store_instr);
                    }
                }
            }
        }
};

class Backend::LIR::PrivilegedFunction : public Backend::LIR::Function {
    public:
        explicit PrivilegedFunction(const std::string &function_name, std::vector<std::shared_ptr<Backend::Variable>> &&params) : Function(std::move(function_name), std::move(params)) {
            this->function_type = FunctionType::PRIVILEGED;
        };
};

namespace Backend::LIR {
    extern inline const std::array<std::shared_ptr<PrivilegedFunction>, 3> privileged_functions = {
        std::make_shared<PrivilegedFunction>("putf", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::STRING_PTR, VariableWide::LOCAL)}),
        std::make_shared<PrivilegedFunction>("putint", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::INT32, VariableWide::LOCAL)}),
        std::make_shared<PrivilegedFunction>("putfloat", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::FLOAT, VariableWide::LOCAL)}),
    };
}

class Backend::LIR::Module : public std::enable_shared_from_this<Backend::LIR::Module> {
    public:
        std::shared_ptr<Mir::Module> llvm_module;
        std::unordered_map<std::string, std::shared_ptr<Backend::LIR::Function>> functions_index;
        std::vector<std::shared_ptr<Backend::LIR::Function>> functions;
        std::shared_ptr<Backend::DataSection> global_data;

        explicit Module(const std::shared_ptr<Mir::Module> &llvm_module) : llvm_module(llvm_module) {
            load_global_data();
            load_functions_and_blocks();
            for (const std::shared_ptr<Mir::Function> &llvm_function : llvm_module->get_functions()) {
                std::shared_ptr<Backend::LIR::Function> function = functions_index[llvm_function->get_name()];
                load_functional_variables(llvm_function, function);
                load_instructions(llvm_function, function);
            }
        }

        inline void add_function(const std::shared_ptr<Backend::LIR::Function> &function) {
            functions.push_back(function);
            functions_index[function->name] = function;
        }
    private:
        void load_global_data()  {
            this->global_data = std::make_shared<Backend::DataSection>();
            this->global_data->load_global_variables(llvm_module->get_global_variables());
            this->global_data->load_global_variables(llvm_module->get_const_strings());
        }

        /*
         * Handle parameters and allocated variables in the function.
         */
        void load_functional_variables(const std::shared_ptr<Mir::Function> &llvm_function, std::shared_ptr<Backend::LIR::Function> &mir_function);

        /*
         * Load privileged/unprivileged functions & blocks into the module.
         * Only create the function and block objects.
         * Instructions and variables will be loaded later.
         */
        void load_functions_and_blocks();

        /*
         * Get `shared_ptr` of a variable stored in function/global_data by name.
         * Return `nullptr` if it finds nothing.
         */
        std::shared_ptr<Backend::Variable> find_variable(const std::string &name, const std::shared_ptr<Backend::LIR::Function> &function) {
            if (function->variables.find(name) != function->variables.end())
                return function->variables[name];
            if (global_data->global_variables.find(name) != global_data->global_variables.end())
                return global_data->global_variables[name];
            return nullptr;
        }

        /*
         * If `value` is a constant, return a `FloatValue` or `IntValue` object.
         * If `value` is a variable, return a `Variable` object.
         * If `value` is not found, return `nullptr`.
         */
        std::shared_ptr<Backend::Operand> find_operand(const std::shared_ptr<Mir::Value> &value, const std::shared_ptr<Backend::LIR::Function> &function) {
            const std::string &name = value->get_name();
            if (value->is_constant())
                if (value->get_type()->is_float())
                    return std::make_shared<Backend::FloatValue>(std::any_cast<double>(std::static_pointer_cast<Mir::ConstFloat>(std::static_pointer_cast<Mir::Init::Constant>(value)->get_const_value())->get_constant_value()));
                else
                    return std::make_shared<Backend::IntValue>(std::any_cast<int32_t>(std::static_pointer_cast<Mir::ConstInt>(std::static_pointer_cast<Mir::Init::Constant>(value)->get_const_value())->get_constant_value()));
            return find_variable(name, function);
        }

        /*
         * Ensure the result is a variable object.
         * If it's a constant, create a temporary variable for it and insert a `load_imm` instruction to the block.
         */
        std::shared_ptr<Backend::Variable> ensure_variable(const std::shared_ptr<Backend::Operand> &value, std::shared_ptr<Backend::LIR::Block> &block) {
            if (value->operand_type == OperandType::CONSTANT) {
                std::shared_ptr<Backend::Constant> constant = std::static_pointer_cast<Backend::Constant>(value);
                std::shared_ptr<Backend::Variable> temp_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("temp_constant"), constant->constant_type, VariableWide::LOCAL);
                block->parent_function.lock()->add_variable(temp_var);
                block->instructions.push_back(std::make_shared<Backend::LIR::LoadI32>(temp_var, constant));
                return temp_var;
            }
            return std::static_pointer_cast<Backend::Variable>(value);
        }

        /*
         * Translate a single instruction from LLVM to LIR.
         */
        void load_instruction(const std::shared_ptr<Mir::Instruction> &instruction, std::shared_ptr<Backend::LIR::Block> &block);

        /*
         * Translate instructions of a single function from LLVM to LIR.
         */
        void load_instructions(const std::shared_ptr<Mir::Function> &llvm_function, std::shared_ptr<Backend::LIR::Function> &mir_function) {
            for (const std::shared_ptr<Mir::Block> &llvm_block : llvm_function->get_blocks()) {
                std::shared_ptr<Backend::LIR::Block> mir_block = mir_function->blocks_index[llvm_block->get_name()];
                for (const std::shared_ptr<Mir::Instruction> &llvm_instruction : llvm_block->get_instructions())
                    load_instruction(llvm_instruction, mir_block);
            }
        }
};

#endif
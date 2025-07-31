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
#include "Pass/Analysis.h"
#include "Pass/Analyses/ControlFlowGraph.h"
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
        LOAD, LOAD_IMM, LOAD_ADDR,
        FLOAD, LOAD_FLOAT_IMM,
        I2F, F2I,
        STORE, FSTORE,
        CALL,
        RETURN,
        JUMP,
        EQUAL, NOT_EQUAL, GREATER, GREATER_EQUAL, LESS, LESS_EQUAL,
        EQUAL_ZERO, NOT_EQUAL_ZERO, GREATER_ZERO, GREATER_EQUAL_ZERO, LESS_ZERO, LESS_EQUAL_ZERO,
        BITWISE_AND, BITWISE_OR, BITWISE_XOR, BITWISE_NOT,
        SHIFT_LEFT,
        SHIFT_RIGHT,
        MOVE, FMOVE,
    };
    class IntArithmetic;
    class FloatArithmetic;
    class LoadIntImm;
    class LoadFloatImm;
    class Call;
    class LoadAddress;
    class LoadInt;
    class LoadFloat;
    class StoreInt;
    class StoreFloat;
    class Convert;
    class BranchInstruction;
    class FBranchInstruction;
    class Jump;
    class Return;
    class Move;
};

namespace Backend::Utils {
    [[nodiscard]] inline Backend::LIR::InstructionType llvm_to_lir(const Mir::IntBinary::Op &op)  {
        switch (op) {
            case Mir::IntBinary::Op::ADD: return Backend::LIR::InstructionType::ADD;
            case Mir::IntBinary::Op::SUB: return Backend::LIR::InstructionType::SUB;
            case Mir::IntBinary::Op::MUL: return Backend::LIR::InstructionType::MUL;
            case Mir::IntBinary::Op::DIV: return Backend::LIR::InstructionType::DIV;
            case Mir::IntBinary::Op::MOD: return Backend::LIR::InstructionType::MOD;
            default: throw std::invalid_argument("Invalid operation");
        }
    }

    [[nodiscard]] inline Backend::LIR::InstructionType llvm_to_lir(const Mir::FloatBinary::Op &op)  {
        switch (op) {
            case Mir::FloatBinary::Op::ADD: return Backend::LIR::InstructionType::FADD;
            case Mir::FloatBinary::Op::SUB: return Backend::LIR::InstructionType::FSUB;
            case Mir::FloatBinary::Op::MUL: return Backend::LIR::InstructionType::FMUL;
            case Mir::FloatBinary::Op::DIV: return Backend::LIR::InstructionType::FDIV;
            case Mir::FloatBinary::Op::MOD: return Backend::LIR::InstructionType::FMOD;
            default: throw std::invalid_argument("Invalid operation");
        }
    }

    [[nodiscard]] inline std::string to_string(const Backend::LIR::InstructionType type) {
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
            case Backend::LIR::InstructionType::LOAD_ADDR: return "&";
            default: return "";
        }
    }

    template <typename T>
    [[nodiscard]] inline T compute(const Backend::LIR::InstructionType &op, T lhs, T rhs) {
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

    [[nodiscard]] inline std::string unique_name(const std::string &prefix = "") {
        static size_t counter = 0;
        return "%%" + prefix + std::to_string(counter++);
    }

    [[nodiscard]] inline Backend::LIR::InstructionType cmp_to_lir(const Backend::Comparison::Type type) {
        switch (type) {
            case Backend::Comparison::Type::EQUAL: return Backend::LIR::InstructionType::EQUAL;
            case Backend::Comparison::Type::NOT_EQUAL: return Backend::LIR::InstructionType::NOT_EQUAL;
            case Backend::Comparison::Type::LESS: return Backend::LIR::InstructionType::LESS;
            case Backend::Comparison::Type::LESS_EQUAL: return Backend::LIR::InstructionType::LESS_EQUAL;
            case Backend::Comparison::Type::GREATER: return Backend::LIR::InstructionType::GREATER;
            case Backend::Comparison::Type::GREATER_EQUAL: return Backend::LIR::InstructionType::GREATER_EQUAL;
            default: throw std::invalid_argument("Invalid operation");
        }
    }

    [[nodiscard]] inline Backend::LIR::InstructionType cmp_to_lir_zero(const Backend::Comparison::Type type) {
        switch (type) {
            case Backend::Comparison::Type::EQUAL: return Backend::LIR::InstructionType::EQUAL_ZERO;
            case Backend::Comparison::Type::NOT_EQUAL: return Backend::LIR::InstructionType::NOT_EQUAL_ZERO;
            case Backend::Comparison::Type::LESS: return Backend::LIR::InstructionType::LESS_ZERO;
            case Backend::Comparison::Type::LESS_EQUAL: return Backend::LIR::InstructionType::LESS_EQUAL_ZERO;
            case Backend::Comparison::Type::GREATER: return Backend::LIR::InstructionType::GREATER_ZERO;
            case Backend::Comparison::Type::GREATER_EQUAL: return Backend::LIR::InstructionType::GREATER_EQUAL_ZERO;
            default: throw std::invalid_argument("Invalid operation");
        }
    }
};

class Backend::LIR::Instruction {
    public:
        InstructionType type;
        std::weak_ptr<Backend::LIR::Block> parent_block;

        virtual ~Instruction() = default;
        Instruction(InstructionType type) : type(type) {}

        virtual std::shared_ptr<Backend::Variable> get_defined_variable() const { return nullptr; }
        std::shared_ptr<Backend::Variable> get_defined_variable(bool (*is_consistent)(const Backend::VariableType &type)) const {
            std::shared_ptr<Backend::Variable> variable = get_defined_variable();
            if (variable && is_consistent(variable->workload_type)) {
                return variable;
            } else return nullptr;
        }
        virtual void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) {}
        virtual std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const { return {}; }
        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables(bool (*is_consistent)(const Backend::VariableType &type)) const {
            std::vector<std::shared_ptr<Backend::Variable>> result = get_used_variables();
            result.erase(std::remove_if(result.begin(), result.end(), [is_consistent](const std::shared_ptr<Backend::Variable> &var) {
                return !is_consistent(var->workload_type);
            }), result.end());
            return result;
        }
        virtual void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) {}

        virtual std::string to_string() const = 0;
};

class Backend::LIR::Block {
    public:
        std::string name;
        std::vector<std::shared_ptr<Backend::LIR::Instruction>> instructions;
        std::vector<std::shared_ptr<Backend::LIR::Block>> predecessors;
        std::vector<std::shared_ptr<Backend::LIR::Block>> successors;
        std::weak_ptr<Backend::LIR::Function> parent_function;

        std::unordered_set<std::string> live_in;
        std::unordered_set<std::string> live_out;

        explicit Block(const std::string &block_name) : name(std::move(block_name)) {};
        explicit Block(const std::string &&block_name) : name(std::move(block_name)) {};
};

class Backend::LIR::Function {
    public:
        std::string name;
        bool is_caller{false};
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

        void add_variable(const std::shared_ptr<Backend::Variable> &variable) {
            if (variables.find(variable->name) == variables.end())
                variables[variable->name] = variable;
        }

        void remove_variable(const std::shared_ptr<Backend::Variable> &variable) {
            variables.erase(variable->name);
        }

        [[nodiscard]] std::shared_ptr<Backend::Variable> find_variable(const std::string &name) const {
            std::map<std::string, std::shared_ptr<Backend::Variable>>::const_iterator it = variables.find(name);
            if (it != variables.end()) {
                return it->second;
            }
            return nullptr;
        }

        void add_block(const std::shared_ptr<Backend::LIR::Block> &block) {
            blocks.push_back(block);
            blocks_index[block->name] = block;
        }

        template <typename T_store, typename T_load>
        void spill(std::shared_ptr<Backend::Variable> &local_variable);
        template<bool (*is_consistent)(const Backend::VariableType &type)>
        void analyze_live_variables();

        [[nodiscard]] std::string to_string() const {
            std::ostringstream oss;
            oss << "Function: " << name << "\n";
            for (const std::shared_ptr<Backend::LIR::Block> &block : blocks) {
                oss << "  " << block->name << "\n";
                for (const auto &instr : block->instructions) {
                    oss << "    " << instr->to_string() << "\n";
                }
            }
            return oss.str();
        }

        [[nodiscard]] std::string live_variables() {
            std::ostringstream oss;
            oss << "Function: " << name << "\n";
            for (const std::shared_ptr<Backend::LIR::Block> &block: blocks) {
                oss << "\nBlock: " << block->name << "\n";
                oss << "  Live In: ";
                for (const std::string &var_name : block->live_in) {
                    oss << var_name << " ";
                }
                oss << "\n";
                oss << "  Live Out: ";
                for (const std::string &var_name : block->live_out) {
                    oss << var_name << " ";
                }
            }
            oss << "\n";
            return oss.str();
        }
    private:
        template<bool (*is_consistent)(const Backend::VariableType &type)>
        bool analyze_live_variables(std::shared_ptr<Backend::LIR::Block> &block, std::unordered_set<std::string> &visited);
};

class Backend::LIR::PrivilegedFunction : public Backend::LIR::Function {
    public:
        explicit PrivilegedFunction(const std::string &function_name, std::vector<std::shared_ptr<Backend::Variable>> &&params) : Function(std::move(function_name), std::move(params)) {
            this->function_type = FunctionType::PRIVILEGED;
        };
};

namespace Backend::LIR {
    extern inline const std::array<std::shared_ptr<PrivilegedFunction>, 11> privileged_functions = {
        std::make_shared<PrivilegedFunction>("putf", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::STRING_PTR, VariableWide::LOCAL)}),
        std::make_shared<PrivilegedFunction>("getint", std::vector<std::shared_ptr<Backend::Variable>>{}),
        std::make_shared<PrivilegedFunction>("getch", std::vector<std::shared_ptr<Backend::Variable>>{}),
        std::make_shared<PrivilegedFunction>("getfloat", std::vector<std::shared_ptr<Backend::Variable>>{}),
        std::make_shared<PrivilegedFunction>("getarray", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::INT32_PTR, VariableWide::LOCAL)}),
        std::make_shared<PrivilegedFunction>("getfarray", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::FLOAT_PTR, VariableWide::LOCAL)}),
        std::make_shared<PrivilegedFunction>("putint", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::INT32, VariableWide::LOCAL)}),
        std::make_shared<PrivilegedFunction>("putch", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::INT32, VariableWide::LOCAL)}),
        std::make_shared<PrivilegedFunction>("putfloat", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::FLOAT, VariableWide::LOCAL)}),
        std::make_shared<PrivilegedFunction>("putarray", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::INT32, VariableWide::LOCAL), std::make_shared<Backend::Variable>("%1", Backend::VariableType::INT32_PTR, VariableWide::LOCAL)}),
        std::make_shared<PrivilegedFunction>("putfarray", std::vector<std::shared_ptr<Backend::Variable>>{std::make_shared<Backend::Variable>("%0", Backend::VariableType::FLOAT, VariableWide::LOCAL), std::make_shared<Backend::Variable>("%1", Backend::VariableType::FLOAT_PTR, VariableWide::LOCAL)}),
    };
}

class Backend::LIR::Module : public std::enable_shared_from_this<Backend::LIR::Module> {
    public:
        std::shared_ptr<Mir::Module> llvm_module;
        std::unordered_map<std::string, std::shared_ptr<Backend::LIR::Function>> functions_index;
        std::vector<std::shared_ptr<Backend::LIR::Function>> functions;
        std::shared_ptr<Backend::DataSection> global_data;

        explicit Module(const std::shared_ptr<Mir::Module> &llvm_module) : llvm_module(llvm_module), cfg(Pass::get_analysis_result<Pass::ControlFlowGraph>(llvm_module)) {
            load_global_data();
            load_functions_and_blocks();
            for (const std::shared_ptr<Mir::Function> &llvm_function : llvm_module->get_functions()) {
                std::shared_ptr<Backend::LIR::Function> function = functions_index[llvm_function->get_name()];
                load_functional_variables(llvm_function, function);
                load_instructions(llvm_function, function);
                clear_variables(function);
            }
        }

        inline void add_function(const std::shared_ptr<Backend::LIR::Function> &function) {
            functions.push_back(function);
            functions_index[function->name] = function;
        }

        [[nodiscard]] std::string to_string() const {
            std::ostringstream oss;
            for (const std::shared_ptr<Backend::LIR::Function> &function : functions)
                oss << function->to_string() << "\n";
            return oss.str();
        }
    private:
        const std::shared_ptr<Pass::ControlFlowGraph> cfg;

        void load_global_data()  {
            this->global_data = std::make_shared<Backend::DataSection>();
            this->global_data->load_global_variables(llvm_module->get_global_variables());
            this->global_data->load_global_variables(llvm_module->get_const_strings());
        }

        /*
         * Handle parameters and allocated variables in the function.
         */
        void load_functional_variables(const std::shared_ptr<Mir::Function> &llvm_function, std::shared_ptr<Backend::LIR::Function> &lir_function);

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
            if (value->is_constant()) {
                if (value->get_type()->is_float())
                    return std::make_shared<Backend::FloatValue>(
                        static_cast<double>(
                            std::static_pointer_cast<Mir::ConstFloat>(value)->get_constant_value()
                        )
                    );
                else
                    return std::make_shared<Backend::IntValue>(
                        static_cast<int32_t>(
                            std::static_pointer_cast<Mir::ConstInt>(value)->get_constant_value()
                        )
                    );
            }
            std::shared_ptr<Backend::Variable> var = find_variable(name, function);
            if (!var)
                log_error("%s not found in function %s", name.c_str(), function->name.c_str());
            return var;
        }

        /*
         * Ensure the result is a variable object.
         * If it's a constant, create a temporary variable for it and insert a `load_imm` instruction to the block.
         */
        std::shared_ptr<Backend::Variable> ensure_variable(const std::shared_ptr<Backend::Operand> &value, std::shared_ptr<Backend::LIR::Block> &block);

        /*
         * Translate a single instruction from LLVM to LIR.
         */
        void load_instruction(const std::shared_ptr<Mir::Instruction> &instruction, std::shared_ptr<Backend::LIR::Block> &block);

        /*
         * Translate instructions of a single function from LLVM to LIR.
         */
        void load_instructions(const std::shared_ptr<Mir::Function> &llvm_function, std::shared_ptr<Backend::LIR::Function> &lir_function) {
            for (const std::shared_ptr<Mir::Block> &llvm_block : cfg->reverse_post_order(llvm_function)) {
                log_debug("Converting %s in function %s", llvm_block->get_name().c_str(), llvm_function->get_name().c_str());
                std::shared_ptr<Backend::LIR::Block> lir_block = lir_function->blocks_index[llvm_block->get_name()];
                for (const std::shared_ptr<Mir::Instruction> &llvm_instruction : llvm_block->get_instructions())
                    load_instruction(llvm_instruction, lir_block);
            }
        }

        /*
         * Clear all cmp/ptr in the function.
         */
        void clear_variables(std::shared_ptr<Backend::LIR::Function> &lir_function) {
            for (std::map<std::string, std::shared_ptr<Backend::Variable>>::iterator it = lir_function->variables.begin(); it != lir_function->variables.end(); )
                if (it->second->var_type == Backend::Variable::Type::PTR || it->second->var_type == Backend::Variable::Type::CMP)
                    it = lir_function->variables.erase(it);
                else it++;
        }

        template<typename StoreInst, Backend::VariableType PTR>
        void load_store_instruction(const std::shared_ptr<Backend::Variable> &store_to, const std::shared_ptr<Backend::Variable> &store_from, std::shared_ptr<Backend::LIR::Block> &lir_block);
        template<typename LoadInst>
        void load_load_instruction(const std::shared_ptr<Backend::Variable> &load_from, const std::shared_ptr<Backend::Variable> &load_to, std::shared_ptr<Backend::LIR::Block> &lir_block);
        std::shared_ptr<Backend::Variable> load_addr(const std::shared_ptr<Backend::Pointer> &load_from, std::shared_ptr<Backend::LIR::Block> &lir_block);
};

#endif
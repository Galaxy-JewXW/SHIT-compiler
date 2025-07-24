#ifndef BACKEND_MIR_INSTRUCTIONS_H
#define BACKEND_MIR_INSTRUCTIONS_H

#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include "Backend/LIR/LIR.h"
#include "Utils/Log.h"

class Backend::LIR::IntArithmetic : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> lhs;
        std::shared_ptr<Operand> rhs;
        std::shared_ptr<Variable> result;

        IntArithmetic(const InstructionType type, const std::shared_ptr<Variable> &lhs, const std::shared_ptr<Operand> &rhs, const std::shared_ptr<Variable> &result) : Backend::LIR::Instruction(type), lhs(lhs), rhs(rhs), result(result) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << result->to_string() << " = " << lhs->to_string() << " " << Backend::Utils::to_string(type) << " " << rhs->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return result; }

        void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) override { result = var; }

        std::vector<std::shared_ptr<Variable>> get_used_variables() const override {
            std::vector<std::shared_ptr<Backend::Variable>> used;
            used.push_back(std::static_pointer_cast<Variable>(lhs));
            if (rhs->operand_type == OperandType::VARIABLE)
                used.push_back(std::static_pointer_cast<Variable>(rhs));
            return used;
        }

        void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) override {
            if (lhs == original) lhs = update_to;
            if (rhs->operand_type == OperandType::VARIABLE && std::static_pointer_cast<Variable>(rhs) == original) rhs = update_to;
        }
};

class Backend::LIR::FloatArithmetic : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> lhs;
        std::shared_ptr<Operand> rhs;
        std::shared_ptr<Variable> result;

        FloatArithmetic(InstructionType type, const std::shared_ptr<Variable> &lhs, const std::shared_ptr<Operand> &rhs, const std::shared_ptr<Variable> &result) : Backend::LIR::Instruction(type), lhs(lhs), rhs(rhs), result(result) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << result->to_string() << " = " << lhs->to_string() << " " << Backend::Utils::to_string(type) << " " << rhs->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return result; }

        void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) override { result = var; }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            std::vector<std::shared_ptr<Backend::Variable>> used;
            used.push_back(std::static_pointer_cast<Variable>(lhs));
            if (rhs->operand_type == OperandType::VARIABLE)
                used.push_back(std::static_pointer_cast<Variable>(rhs));
            return used;
        }

        void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) override {
            if (lhs == original) lhs = update_to;
            if (rhs->operand_type == OperandType::VARIABLE && std::static_pointer_cast<Variable>(rhs) == original) rhs = update_to;
        }
};

/*
 * Load an immediate 32-bit integer value into a (virtual) register.
 */
class Backend::LIR::LoadIntImm : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<IntValue> immediate;
        std::shared_ptr<Variable> var_in_reg;

        LoadIntImm(const std::shared_ptr<Variable> &var_in_reg, const std::shared_ptr<IntValue> &immediate) : Backend::LIR::Instruction(InstructionType::LOAD_IMM), immediate(immediate), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << var_in_reg->to_string() << " = " << immediate->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return var_in_reg; }

        void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) override { var_in_reg = var; }
};

/*
 * Load an immediate 32-bit float value into a (virtual) register.
 */
class Backend::LIR::LoadFloatImm : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<FloatValue> immediate;
        std::shared_ptr<Variable> var_in_reg;

        LoadFloatImm(const std::shared_ptr<Variable> &var_in_reg, const std::shared_ptr<FloatValue> &immediate) : Backend::LIR::Instruction(InstructionType::LOAD_FLOAT_IMM), immediate(immediate), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << var_in_reg->to_string() << " = " << immediate->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return var_in_reg; }

        void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) override { var_in_reg = var; }
};

/*
 * Not only global variables but functional variables can be load.
 * For functional variables, `load` will be translated like `add x0, sp, x0`
 */
class Backend::LIR::LoadAddress : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> addr;

        LoadAddress(const std::shared_ptr<Variable> &var_in_mem, const std::shared_ptr<Variable> &addr) : Backend::LIR::Instruction(InstructionType::LOAD_ADDR), var_in_mem(var_in_mem), addr(addr) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << addr->to_string() << " = &" << var_in_mem->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return addr; }
};

class Backend::LIR::Move : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> source;
        std::shared_ptr<Variable> target;

        Move(const std::shared_ptr<Variable> &source, const std::shared_ptr<Variable> &target) : Backend::LIR::Instruction(InstructionType::MOVE), source(source), target(target) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << target->to_string() << " = " << source->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return target; }

        void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) override { target = var; }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            return {source};
        }

        void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) override {
            if (source == original) source = update_to;
        }
};

/*
 * If the callee function returns no value, the `result` is `nullptr`.
 */
class Backend::LIR::Call : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Backend::Variable> result;
        std::shared_ptr<Backend::LIR::Function> function;
        std::vector<std::shared_ptr<Backend::Variable>> arguments;

        Call(const std::shared_ptr<Backend::Variable> &result, const std::shared_ptr<Backend::LIR::Function> &function, const std::vector<std::shared_ptr<Backend::Variable>> &arguments) : Backend::LIR::Instruction(Backend::LIR::InstructionType::CALL), result(result), function(function), arguments(arguments) {}
        Call(const std::shared_ptr<Backend::LIR::Function> &function, const std::vector<std::shared_ptr<Backend::Variable>> &arguments) : Backend::LIR::Instruction(Backend::LIR::InstructionType::CALL), function(function), arguments(arguments) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            if (function) oss << function->name;
            else oss << Backend::Utils::to_string(type);
            oss << "(";
            for (const auto &arg : arguments) {
                oss << arg->to_string() << ", ";
            }
            oss << ")";
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return result; }

        void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) override {
            if (result) result = var;
        }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            std::vector<std::shared_ptr<Backend::Variable>> used;
            for (const std::shared_ptr<Backend::Variable> &arg : arguments)
                used.push_back(arg);
            return used;
        }

        void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) override {
            for (auto &arg : arguments) {
                if (arg == original) arg = update_to;
            }
        }
};

class Backend::LIR::LoadInt : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> var_in_reg;
        int64_t offset{0};

        LoadInt(const std::shared_ptr<Variable> &var_in_mem, const std::shared_ptr<Variable> &var_in_reg, int64_t offset) : Instruction(InstructionType::LOAD), var_in_mem(var_in_mem), var_in_reg(var_in_reg), offset(offset) {}
        LoadInt(const std::shared_ptr<Variable> &var_in_mem, const std::shared_ptr<Variable> &var_in_reg) : Instruction(InstructionType::LOAD), var_in_mem(var_in_mem), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "load from " << var_in_mem->to_string() << " + " << offset << " to " << var_in_reg->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return var_in_reg; }

        void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) override { var_in_reg = var; }
};

class Backend::LIR::LoadFloat : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> var_in_reg;
        int32_t offset{0};

        LoadFloat(const std::shared_ptr<Variable> &var_in_mem, const std::shared_ptr<Variable> &var_in_reg, int32_t offset) : Instruction(InstructionType::FLOAD), var_in_mem(var_in_mem), var_in_reg(var_in_reg), offset(offset) {}
        LoadFloat(const std::shared_ptr<Variable> &var_in_mem, const std::shared_ptr<Variable> &var_in_reg) : Instruction(InstructionType::FLOAD), var_in_mem(var_in_mem), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "load from " << var_in_mem->to_string() << " + " << offset << " to " << var_in_reg->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return var_in_reg; }

        void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) override { var_in_reg = var; }
};

class Backend::LIR::StoreInt : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> var_in_reg;
        int32_t offset{0};

        StoreInt(const std::shared_ptr<Variable> &var_in_mem, const std::shared_ptr<Variable> &var_in_reg, int32_t offset) : Instruction(InstructionType::STORE), var_in_mem(var_in_mem), var_in_reg(var_in_reg), offset(offset) {}
        StoreInt(const std::shared_ptr<Variable> &var_in_mem, const std::shared_ptr<Variable> &var_in_reg) : Instruction(InstructionType::STORE), var_in_mem(var_in_mem), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "store from " << var_in_reg->to_string() << " to " << var_in_mem->to_string() << " + " << offset;
            return oss.str();
        }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            return {var_in_reg};
        }

        void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) override {
            if (var_in_reg == original) var_in_reg = update_to;
        }
};

class Backend::LIR::StoreFloat : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> var_in_reg;
        int32_t offset{0};

        StoreFloat(const std::shared_ptr<Variable> &var_in_mem, const std::shared_ptr<Variable> &var_in_reg, int32_t offset) : Instruction(InstructionType::FSTORE), var_in_mem(var_in_mem), var_in_reg(var_in_reg), offset(offset) {}
        StoreFloat(const std::shared_ptr<Variable> &var_in_mem, const std::shared_ptr<Variable> &var_in_reg) : Instruction(InstructionType::FSTORE), var_in_mem(var_in_mem), var_in_reg(var_in_reg) {}

        std::string to_string() const override {
            std::ostringstream oss;
            oss << "store from " << var_in_reg->to_string() << " to " << var_in_mem->to_string() << " + " << offset;
            return oss.str();
        }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            return {var_in_reg};
        }

        void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) override {
            if (var_in_reg == original) var_in_reg = update_to;
        }
};

class Backend::LIR::Jump : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Backend::LIR::Block> target_block;
        Jump(const std::shared_ptr<Backend::LIR::Block> &target_block) : Instruction(InstructionType::JUMP), target_block(target_block) {}

        std::string to_string() const override {
            std::ostringstream oss;
            oss << "goto " << target_block->name;
            return oss.str();
        }
};

/*
 * Condition jump.
 * If `rhs` refers to `zero`, then `rhs = nullptr`.
 */
class Backend::LIR::BranchInstruction : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Backend::Variable> lhs;
        std::shared_ptr<Backend::Variable> rhs;
        std::shared_ptr<Backend::LIR::Block> target_block;

        BranchInstruction(const InstructionType type, const std::shared_ptr<Backend::Variable> &lhs, const std::shared_ptr<Backend::Variable> &rhs, const std::shared_ptr<Backend::LIR::Block> &target_block) : Instruction(type), lhs(lhs), rhs(rhs), target_block(target_block) {}
        BranchInstruction(const InstructionType type, const std::shared_ptr<Backend::Variable> &lhs, const std::shared_ptr<Backend::LIR::Block> &target_block) : Instruction(type), lhs(lhs), target_block(target_block) {}

        std::string to_string() const override {
            std::ostringstream oss;
            oss << lhs->to_string()
                << " " << Backend::Utils::to_string(type)
                << " " << (rhs ? rhs->to_string() : "0")
                << " goto " << target_block->name;
            return oss.str();
        }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            if (rhs) {
                return {lhs, rhs};
            } return {lhs};
        }

        void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) override {
            if (lhs == original) lhs = update_to;
            if (rhs && rhs == original) rhs = update_to;
        }
};

class Backend::LIR::FBranchInstruction : public Backend::LIR::BranchInstruction {
    public:
        FBranchInstruction(const InstructionType type, const std::shared_ptr<Backend::Variable> &lhs, const std::shared_ptr<Backend::Variable> &rhs, const std::shared_ptr<Backend::LIR::Block> &target_block) : BranchInstruction(type, lhs, rhs, target_block) {}
        FBranchInstruction(const InstructionType type, const std::shared_ptr<Backend::Variable> &lhs, const std::shared_ptr<Backend::LIR::Block> &target_block) : BranchInstruction(type, lhs, target_block) {}
};

/*
 * Convert i32 to float (or vise versa).
 */
class Backend::LIR::Convert : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Backend::Variable> source;
        std::shared_ptr<Backend::Variable> dest;

        Convert(const InstructionType type, const std::shared_ptr<Backend::Variable> &source, const std::shared_ptr<Backend::Variable> &dest) : Instruction(type), source(source), dest(dest) {}

        std::string to_string() const override {
            std::ostringstream oss;
            oss << source->to_string()
                << " " << Backend::Utils::to_string(type)
                << " " << dest->to_string();
            return oss.str();
        }

        std::shared_ptr<Backend::Variable> get_defined_variable() const override { return dest; }
        void update_defined_variable(const std::shared_ptr<Backend::Variable> &var) override { dest = var; }
        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override { return {source}; }
        void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) override {
            if (source == original) source = update_to;
        }
};

/*
 * If no return value is expected, the `return_value` is `nullptr`.
 */
class Backend::LIR::Return : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> return_value;

        Return(const std::shared_ptr<Variable> &return_value) : Instruction(Backend::LIR::InstructionType::RETURN), return_value(return_value) {}
        Return() : Instruction(Backend::LIR::InstructionType::RETURN) {}

        std::string to_string() const override {
            std::ostringstream oss;
            oss << "return " << (return_value ? return_value->to_string() : "void");
            return oss.str();
        }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            return return_value ? std::vector<std::shared_ptr<Backend::Variable>>{return_value} : std::vector<std::shared_ptr<Backend::Variable>>();
        }

        void update_used_variable(const std::shared_ptr<Backend::Variable> &original, const std::shared_ptr<Backend::Variable> &update_to) override {
            if (return_value == original) return_value = update_to;
        }
};
#endif
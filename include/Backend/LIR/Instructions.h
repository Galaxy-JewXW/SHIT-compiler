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

        IntArithmetic(InstructionType type, const std::shared_ptr<Variable> &lhs, const std::shared_ptr<Operand> &rhs, const std::shared_ptr<Variable> &result) : Backend::LIR::Instruction(type), lhs(lhs), rhs(rhs), result(result) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << result->to_string() << " = " << lhs->to_string() << " " << Backend::Utils::to_string(type) << " " << rhs->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return result; }

        std::vector<std::shared_ptr<Variable>> get_used_variables() const override {
            std::vector<std::shared_ptr<Backend::Variable>> used;
            used.push_back(std::static_pointer_cast<Variable>(lhs));
            if (rhs->operand_type == OperandType::VARIABLE)
                used.push_back(std::static_pointer_cast<Variable>(rhs));
            return used;
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

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            std::vector<std::shared_ptr<Backend::Variable>> used;
            used.push_back(std::static_pointer_cast<Variable>(lhs));
            if (rhs->operand_type == OperandType::VARIABLE)
                used.push_back(std::static_pointer_cast<Variable>(rhs));
            return used;
        }
};

class Backend::LIR::LoadI32 : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Constant> immediate;
        std::shared_ptr<Variable> var_in_reg;

        LoadI32(const std::shared_ptr<Variable> &var_in_reg, const std::shared_ptr<Constant> &immediate) : Backend::LIR::Instruction(InstructionType::LOAD_IMM), immediate(immediate), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << var_in_reg->to_string() << " = " << immediate->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return var_in_reg; }
};

/*
 * Not only global variables but functional variables can be load.
 * For functional variables, `load` will be translated like `add x0, sp, x0`
 */
class Backend::LIR::LoadAddress : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> addr;

        LoadAddress(std::shared_ptr<Variable> &var_in_mem, std::shared_ptr<Variable> &addr) : Backend::LIR::Instruction(InstructionType::LOAD_ADDR), var_in_mem(var_in_mem), addr(addr) {}

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

        Move(std::shared_ptr<Variable> &source, std::shared_ptr<Variable> &target) : Backend::LIR::Instruction(InstructionType::MOVE), source(source), target(target) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << target->to_string() << " = " << source->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return target; }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            std::vector<std::shared_ptr<Backend::Variable>> used;
            if (source && source->operand_type == OperandType::VARIABLE)
                used.push_back(std::static_pointer_cast<Variable>(source));
            return used;
        }
};

class Backend::LIR::Call : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Backend::Variable> result;
        std::shared_ptr<Backend::LIR::Function> function;
        std::vector<std::shared_ptr<Backend::Variable>> arguments;

        Call(const std::shared_ptr<Backend::Variable> &result, const std::shared_ptr<Backend::LIR::Function> &function, const std::vector<std::shared_ptr<Backend::Variable>> &arguments) : Backend::LIR::Instruction(Backend::LIR::InstructionType::CALL), result(result), function(function), arguments(arguments) {}
        Call(Backend::LIR::InstructionType type, const std::vector<std::shared_ptr<Backend::Variable>> &arguments) : Backend::LIR::Instruction(type), arguments(arguments) {}

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

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            std::vector<std::shared_ptr<Backend::Variable>> used;
            for (const std::shared_ptr<Backend::Variable> &arg : arguments)
                used.push_back(arg);
            return used;
        }
};

class Backend::LIR::LoadInt : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> var_in_reg;
        int64_t offset{0};

        LoadInt(std::shared_ptr<Variable> var_in_mem, std::shared_ptr<Variable> var_in_reg, int64_t offset) : Instruction(InstructionType::LOAD), var_in_mem(var_in_mem), var_in_reg(var_in_reg), offset(offset) {}
        LoadInt(std::shared_ptr<Variable> var_in_mem, std::shared_ptr<Variable> var_in_reg) : Instruction(InstructionType::LOAD), var_in_mem(var_in_mem), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "load from " << var_in_mem->to_string() << " + " << offset << " to " << var_in_reg->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return var_in_reg; }
};

class Backend::LIR::LoadFloat : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> var_in_reg;
        int64_t offset{0};

        LoadFloat(std::shared_ptr<Variable> var_in_mem, std::shared_ptr<Variable> var_in_reg, int64_t offset) : Instruction(InstructionType::FLOAD), var_in_mem(var_in_mem), var_in_reg(var_in_reg), offset(offset) {}
        LoadFloat(std::shared_ptr<Variable> var_in_mem, std::shared_ptr<Variable> var_in_reg) : Instruction(InstructionType::FLOAD), var_in_mem(var_in_mem), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "load from " << var_in_mem->to_string() << " + " << offset << " to " << var_in_reg->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override { return var_in_reg; }
};

class Backend::LIR::StoreInt : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> var_in_reg;
        int64_t offset{0};

        StoreInt(std::shared_ptr<Variable> var_in_mem, std::shared_ptr<Variable> var_in_reg, int64_t offset) : Instruction(InstructionType::STORE), var_in_mem(var_in_mem), var_in_reg(var_in_reg), offset(offset) {}
        StoreInt(std::shared_ptr<Variable> var_in_mem, std::shared_ptr<Variable> var_in_reg) : Instruction(InstructionType::STORE), var_in_mem(var_in_mem), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "store from " << var_in_reg->to_string() << " to " << var_in_mem->to_string() << " + " << offset;
            return oss.str();
        }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            return {var_in_reg};
        }
};

class Backend::LIR::StoreFloat : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Variable> var_in_reg;
        int64_t offset{0};

        StoreFloat(std::shared_ptr<Variable> var_in_mem, std::shared_ptr<Variable> var_in_reg, int64_t offset) : Instruction(InstructionType::FSTORE), var_in_mem(var_in_mem), var_in_reg(var_in_reg), offset(offset) {}
        StoreFloat(std::shared_ptr<Variable> var_in_mem, std::shared_ptr<Variable> var_in_reg) : Instruction(InstructionType::FSTORE), var_in_mem(var_in_mem), var_in_reg(var_in_reg) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "store from " << var_in_reg->to_string() << " to " << var_in_mem->to_string() << " + " << offset;
            return oss.str();
        }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            return {var_in_reg};
        }
};

class Backend::LIR::JumpInstruction : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Backend::LIR::Block> target_block;
        JumpInstruction(const std::shared_ptr<Backend::LIR::Block> &target_block) : Instruction(InstructionType::JUMP), target_block(target_block) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "goto " << target_block->name;
            return oss.str();
        }
};

class Backend::LIR::BranchInstruction : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Backend::Variable> lhs;
        std::shared_ptr<Backend::Variable> rhs;
        std::shared_ptr<Backend::LIR::Block> target_block;

        BranchInstruction(InstructionType type, std::shared_ptr<Backend::Variable> lhs, std::shared_ptr<Backend::Variable> rhs, const std::shared_ptr<Backend::LIR::Block> &target_block) : Instruction(type), lhs(lhs), rhs(rhs), target_block(target_block) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << lhs->to_string() << " " << Backend::Utils::to_string(type) << " " << rhs->to_string() << " goto " << target_block->name;
            return oss.str();
        }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            std::vector<std::shared_ptr<Backend::Variable>> used;
            used.push_back(std::static_pointer_cast<Variable>(lhs));
            used.push_back(std::static_pointer_cast<Variable>(rhs));
            return used;
        }
};

class Backend::LIR::ReturnInstruction : public Backend::LIR::Instruction {
    public:
        std::shared_ptr<Variable> return_value;

        ReturnInstruction(std::shared_ptr<Variable> return_value) : Instruction(Backend::LIR::InstructionType::RETURN), return_value(return_value) {}
        ReturnInstruction() : Instruction(Backend::LIR::InstructionType::RETURN) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "return " << (return_value ? return_value->to_string() : "void");
            return oss.str();
        }

        std::vector<std::shared_ptr<Backend::Variable>> get_used_variables() const override {
            std::vector<std::shared_ptr<Backend::Variable>> used;
            if (return_value && return_value->operand_type == OperandType::VARIABLE)
                used.push_back(std::static_pointer_cast<Variable>(return_value));
            return used;
        }
};
#endif
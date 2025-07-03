#ifndef BACKEND_MIR_INSTRUCTIONS_H
#define BACKEND_MIR_INSTRUCTIONS_H

#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include "Backend/MIR/MIR.h"
#include "Utils/Log.h"

class Backend::MIR::ArithmeticInstruction : public Backend::MIR::Instruction {
    public:
        std::shared_ptr<Value> lhs;
        std::shared_ptr<Value> rhs;
        std::shared_ptr<Variable> result;

        ArithmeticInstruction(InstructionType type, std::shared_ptr<Value> lhs, std::shared_ptr<Value> rhs, std::shared_ptr<Variable> result) : Backend::MIR::Instruction(type), lhs(lhs), rhs(rhs), result(result) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << result->to_string() << " = " << lhs->to_string() << " " << Backend::Utils::to_string(type) << " " << rhs->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override {
            return result;
        }

        std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> get_used_variables() const override {
            std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> used = std::make_shared<std::vector<std::shared_ptr<Backend::MIR::Variable>>>();
            if (lhs && lhs->value_type == OperandType::VARIABLE)
                used->push_back(std::static_pointer_cast<Variable>(lhs));
            if (rhs && rhs->value_type == OperandType::VARIABLE)
                used->push_back(std::static_pointer_cast<Variable>(rhs));
            return used;
        }
};

class Backend::MIR::CallInstruction : public Backend::MIR::Instruction {
    public:
        std::shared_ptr<Backend::MIR::Variable> result;
        std::shared_ptr<Backend::MIR::Function> function;
        std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Value>>> arguments;

        CallInstruction(const std::shared_ptr<Backend::MIR::Variable> &result, const std::shared_ptr<Backend::MIR::Function> &function, const std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Value>>> &arguments) : Backend::MIR::Instruction(Backend::MIR::InstructionType::CALL), result(result), function(function), arguments(std::move(arguments)) {}
        CallInstruction(Backend::MIR::InstructionType type, const std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Value>>> &arguments) : Backend::MIR::Instruction(type), arguments(std::move(arguments)) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            if (function) oss << function->name;
            else oss << Backend::Utils::to_string(type);
            oss << "(";
            for (const auto &arg : *arguments) {
                oss << arg->to_string() << ", ";
            }
            oss << ")";
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override {
            return result;
        }

        std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> get_used_variables() const override {
            std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> used = std::make_shared<std::vector<std::shared_ptr<Backend::MIR::Variable>>>();
            for (const std::shared_ptr<Backend::MIR::Value> &arg : *arguments) {
                if (arg && arg->value_type == OperandType::VARIABLE)
                    used->push_back(std::static_pointer_cast<Variable>(arg));
            }
            return used;
        }
};

class Backend::MIR::MemoryInstruction : public Backend::MIR::Instruction {
    public:
        std::shared_ptr<Variable> var_in_mem;
        std::shared_ptr<Value> var_in_reg;

        MemoryInstruction(InstructionType type, std::shared_ptr<Variable> var_in_mem, std::shared_ptr<Value> value) : Instruction(type), var_in_mem(std::move(var_in_mem)), var_in_reg(std::move(value)) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << Backend::Utils::to_string(type) << " " << var_in_mem->to_string() << " <- " << var_in_reg->to_string();
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override {
            if (type == InstructionType::LOAD)
                return std::static_pointer_cast<Variable>(var_in_reg);
            return nullptr;
        }

        std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> get_used_variables() const override {
            std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> used = std::make_shared<std::vector<std::shared_ptr<Backend::MIR::Variable>>>();
            if (var_in_mem && var_in_mem->value_type == OperandType::VARIABLE)
                used->push_back(std::static_pointer_cast<Variable>(var_in_mem));
            if (type == InstructionType::STORE && var_in_reg && var_in_reg->value_type == OperandType::VARIABLE)
                used->push_back(std::static_pointer_cast<Variable>(var_in_reg));
            return used;
        }
};

class Backend::MIR::BranchInstruction : public Backend::MIR::Instruction {
    public:
        std::shared_ptr<Backend::MIR::CompareVariable> cond;
        std::shared_ptr<Backend::MIR::Block> target_block;

        BranchInstruction(std::shared_ptr<Backend::MIR::CompareVariable> cond, const std::shared_ptr<Backend::MIR::Block> &target_block) : Instruction(cond->compare_type), cond(std::move(cond)), target_block(std::move(target_block)) {}
        BranchInstruction(InstructionType type, const std::shared_ptr<Backend::MIR::Block> &target_block) : Instruction(type), target_block(std::move(target_block)) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            if (cond) oss << cond->to_string() << " goto ";
            else oss << Backend::Utils::to_string(type) << " ";
            oss << target_block->name;
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override {
            return nullptr;
        }

        std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> get_used_variables() const override {
            std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> used = std::make_shared<std::vector<std::shared_ptr<Backend::MIR::Variable>>>();
            if (!cond)
                return used;
            if (cond->lhs && cond->lhs->value_type == OperandType::VARIABLE)
                used->push_back(std::static_pointer_cast<Variable>(cond->lhs));
            if (cond->rhs && cond->rhs->value_type == OperandType::VARIABLE)
                used->push_back(std::static_pointer_cast<Variable>(cond->rhs));
            return used;
        }
};

class Backend::MIR::ReturnInstruction : public Backend::MIR::Instruction {
    public:
        std::shared_ptr<Value> return_value;

        ReturnInstruction(std::shared_ptr<Value> return_value) : Instruction(Backend::MIR::InstructionType::RETURN), return_value(std::move(return_value)) {}
        ReturnInstruction() : Instruction(Backend::MIR::InstructionType::RETURN) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << "return" << (return_value ? " " + return_value->to_string() : "");
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override {
            return nullptr;
        }

        std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> get_used_variables() const override {
            std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> used = std::make_shared<std::vector<std::shared_ptr<Backend::MIR::Variable>>>();
            if (return_value && return_value->value_type == OperandType::VARIABLE)
                used->push_back(std::static_pointer_cast<Variable>(return_value));
            return used;
        }
};

class Backend::MIR::PhiInstruction : public Backend::MIR::Instruction {
    public:
        std::shared_ptr<Value> move_from;
        std::shared_ptr<Variable> move_to;
        std::shared_ptr<Variable> declare;

        PhiInstruction(std::shared_ptr<Value> move_from, std::shared_ptr<Variable> move_to) : Instruction(Backend::MIR::InstructionType::MOVE), move_from(std::move(move_from)), move_to(std::move(move_to)) {}
        PhiInstruction(std::shared_ptr<Variable> declare) : Instruction(Backend::MIR::InstructionType::PHI), declare(declare) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            if (declare) {
                oss << "phi " << declare->to_string();
            } else {
                oss << "phi " << move_to->to_string() << " = " << move_from->to_string();
            }
            return oss.str();
        }

        std::shared_ptr<Variable> get_defined_variable() const override {
            if (declare)
                return declare;
            return nullptr;
        }

        std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> get_used_variables() const override {
            std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> used = std::make_shared<std::vector<std::shared_ptr<Backend::MIR::Variable>>>();
            if (move_from && move_from->value_type == OperandType::VARIABLE)
                used->push_back(std::static_pointer_cast<Variable>(move_from));
            return used;
        }
};
#endif
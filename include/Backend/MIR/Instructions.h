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
        std::shared_ptr<Value> result;

        ArithmeticInstruction(InstructionType type, std::shared_ptr<Value> lhs, std::shared_ptr<Value> rhs, std::shared_ptr<Value> result) : Backend::MIR::Instruction(type), lhs(std::move(lhs)), rhs(std::move(rhs)), result(std::move(result)) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << result->to_string() << " = " << lhs->to_string() << " " << InstructionType_to_string(type) << " " << rhs->to_string();
            return oss.str();
        }
};

class Backend::MIR::CallInstruction : public Backend::MIR::Instruction {
    public:
        std::shared_ptr<Backend::MIR::Function> function;
        std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Value>>> arguments;

        CallInstruction(const std::shared_ptr<Backend::MIR::Function> function, const std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Value>>> &arguments) : Backend::MIR::Instruction(Backend::MIR::InstructionType::CALL), function(function), arguments(arguments) {}
        CallInstruction(Backend::MIR::InstructionType type, const std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Value>>> &arguments) : Backend::MIR::Instruction(type), arguments(arguments) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            if (function) oss << function->name;
            else oss << InstructionType_to_string(type);
            oss << "(";
            for (const auto &arg : *arguments) {
                oss << arg->to_string() << ", ";
            }
            oss << ")";
            return oss.str();
        }
};

class Backend::MIR::MemoryInstruction : public Backend::MIR::Instruction {
    public:
        std::shared_ptr<Value> var_in_mem;
        std::shared_ptr<Value> var_in_reg;

        MemoryInstruction(InstructionType type, std::shared_ptr<Value> var_in_mem, std::shared_ptr<Value> value) : Instruction(type), var_in_mem(std::move(var_in_mem)), var_in_reg(std::move(value)) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            oss << InstructionType_to_string(type) << " " << var_in_mem->to_string() << ", " << var_in_reg->to_string();
            return oss.str();
        }
};

class Backend::MIR::BranchInstruction : public Backend::MIR::Instruction {
    public:
        std::shared_ptr<Backend::MIR::Variable> cond;
        std::shared_ptr<Backend::MIR::Block> target_block;

        BranchInstruction(std::shared_ptr<Backend::MIR::Variable> cond, const std::shared_ptr<Backend::MIR::Block> &target_block) : Instruction(cond->defined_in->type), cond(std::move(cond)), target_block(std::move(target_block)) {}
        BranchInstruction(InstructionType type, const std::shared_ptr<Backend::MIR::Block> &target_block) : Instruction(type), target_block(std::move(target_block)) {}

        inline std::string to_string() const override {
            std::ostringstream oss;
            if (cond) oss << "(" << cond->defined_in->to_string() << ") ";
            else oss << InstructionType_to_string(type) << " ";
            oss << target_block->name;
            return oss.str();
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
};

#endif
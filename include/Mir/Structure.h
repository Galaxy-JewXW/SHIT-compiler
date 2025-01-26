#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <vector>

#include "Value.h"

namespace Mir::Init {
class Init;
}

namespace Mir {
class GlobalVariable;
class Function;
class Instruction;

class Module {
    std::vector<std::shared_ptr<GlobalVariable>> global_variables;
    std::vector<std::string> const_strings;
    std::vector<std::shared_ptr<Function>> functions;
    std::shared_ptr<Function> main_function;

public:
    explicit Module() = default;

    void add_global_variable(const std::shared_ptr<GlobalVariable> &global_variable) {
        global_variables.emplace_back(global_variable);
    }

    void add_const_string(const std::string &const_string) { const_strings.emplace_back(const_string); }

    void add_function(const std::shared_ptr<Function> &function) { functions.emplace_back(function); }

    void set_main_function(const std::shared_ptr<Function> &main_function) { this->main_function = main_function; }

    [[nodiscard]] std::string to_string() const;
};

class GlobalVariable final : public Value {
    const bool is_constant;
    const std::shared_ptr<Init::Init> init_value;

public:
    GlobalVariable(const std::string &name, const std::shared_ptr<Type::Type> &type, const bool is_constant,
                   const std::shared_ptr<Init::Init> &init_value)
        : Value{name, type}, is_constant{is_constant}, init_value{init_value} {}

    [[nodiscard]] std::string to_string() const override;
};

class Argument final : public Value {
    const int index;

public:
    Argument(const std::string &name, const std::shared_ptr<Type::Type> &type, const int index)
        : Value{name, type}, index{index} {}

    [[nodiscard]] std::string to_string() const override { return type_->to_string() + " " + name_; }
};

class Block;

class Function final : public User {
    const std::shared_ptr<Type::Type> return_type;
    std::vector<std::shared_ptr<Argument>> arguments;
    std::vector<std::shared_ptr<Block>> blocks;

public:
    Function(const std::string &name, const std::shared_ptr<Type::Type> &return_type)
        : User(name, return_type), return_type{return_type} {}

    [[nodiscard]] const std::shared_ptr<Type::Type> &get_return_type() const { return return_type; }

    [[nodiscard]] std::vector<std::shared_ptr<Argument>> get_arguments() const { return arguments; }

    void add_argument(const std::shared_ptr<Argument> &argument) { arguments.emplace_back(argument); }

    void add_block(const std::shared_ptr<Block> &block) { blocks.emplace_back(block); }

    [[nodiscard]] std::string to_string() const override;
};

class Block final : public User {
    std::weak_ptr<Function> parent;
    std::vector<std::shared_ptr<Instruction>> instructions;

public:
    explicit Block(const std::string &name): User(name, Type::Label::label) {}

    static std::shared_ptr<Block> create(const std::string &name, const std::shared_ptr<Function> &function) {
        const auto block = std::make_shared<Block>(name);
        block->set_function(function);
        return block;
    }

    void set_function(const std::shared_ptr<Function> &function) {
        parent = function;
        function->add_block(std::dynamic_pointer_cast<Block>(shared_from_this()));
    }

    [[nodiscard]] std::shared_ptr<Function> get_function() const { return parent.lock(); }

    void add_instruction(const std::shared_ptr<Instruction> &instruction) { instructions.emplace_back(instruction); }

    [[nodiscard]] std::string to_string() const override;
};
}

#endif

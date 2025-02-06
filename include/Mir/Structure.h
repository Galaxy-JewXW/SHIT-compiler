#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <algorithm>
#include <unordered_map>
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
    std::vector<std::shared_ptr<Function>> used_runtime_functions;
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

    [[nodiscard]] size_t get_const_string_size() const { return const_strings.size(); }

    void add_used_runtime_functions(const std::shared_ptr<Function> &function) {
        if (std::find(used_runtime_functions.begin(), used_runtime_functions.end(), function)
            == used_runtime_functions.end()) {
            used_runtime_functions.emplace_back(function);
        }
    }

    void add_function(const std::shared_ptr<Function> &function) { functions.emplace_back(function); }

    [[nodiscard]] std::shared_ptr<Function> get_function(const std::string &name) {
        const auto it = std::find_if(functions.begin(), functions.end(),
                                     [&name](const auto &function) {
                                         return function->get_name() == name;
                                     });
        if (it != functions.end()) { return *it; }
        return nullptr;
    }

    void set_main_function(const std::shared_ptr<Function> &main_function) { this->main_function = main_function; }

    [[nodiscard]] std::string to_string() const;
};

class GlobalVariable final : public Value {
    const bool is_constant;
    const std::shared_ptr<Init::Init> init_value;

public:
    GlobalVariable(const std::string &name, const std::shared_ptr<Type::Type> &type, const bool is_constant,
                   const std::shared_ptr<Init::Init> &init_value)
        : Value{"@" + name, std::make_shared<Type::Pointer>(type)}, is_constant{is_constant}, init_value{init_value} {}

    [[nodiscard]] std::string to_string() const override;
};

class Argument final : public Value {
    int index;

public:
    Argument(const std::string &name, const std::shared_ptr<Type::Type> &type, const int index)
        : Value{name, type}, index{index} {}

    [[nodiscard]] int get_index() const { return index; }

    void set_index(const int index) { this->index = index; }

    [[nodiscard]] std::string to_string() const override { return type_->to_string() + " " + name_; }
};

class Block;

class Function final : public User {
    const std::shared_ptr<Type::Type> return_type;
    std::vector<std::shared_ptr<Argument>> arguments;
    std::vector<std::shared_ptr<Block>> blocks;
    const bool is_runtime_function;

public:
    Function(const std::string &name, const std::shared_ptr<Type::Type> &return_type,
             const bool is_runtime_function = false)
        : User(name, return_type), return_type{return_type}, is_runtime_function{is_runtime_function} {}

    template<typename... Types>
    static std::shared_ptr<Function> create(const std::string &name,
                                            const std::shared_ptr<Type::Type> &return_type,
                                            Types... argument_types) {
        const auto &func = std::make_shared<Function>(name, return_type, true);
        std::vector<std::shared_ptr<Type::Type>> arguments_types{argument_types...};
        for (size_t i = 0; i < arguments_types.size(); ++i) {
            const auto &arg = std::make_shared<Argument>("%" + std::to_string(i), arguments_types[i], i);
            func->add_argument(arg);
        }
        return func;
    }

    // SysY 定义的运行时库函数
    const static std::unordered_map<std::string, std::shared_ptr<Function>> runtime_functions;

    [[nodiscard]] const std::shared_ptr<Type::Type> &get_return_type() const { return return_type; }

    [[nodiscard]] std::vector<std::shared_ptr<Argument>> get_arguments() const { return arguments; }

    void add_argument(const std::shared_ptr<Argument> &argument) { arguments.emplace_back(argument); }

    void add_block(const std::shared_ptr<Block> &block) { blocks.emplace_back(block); }

    [[nodiscard]] std::vector<std::shared_ptr<Block>> &get_blocks() { return blocks; }

    // 清除流图后需要更新基本块和指令的id
    void update_id();

    [[nodiscard]] std::string to_string() const override;
};


class Block final : public User {
    std::weak_ptr<Function> parent;
    std::vector<std::shared_ptr<Instruction>> instructions;
    // 标记基本块是否被删除
    bool deleted{false};

public:
    explicit Block(const std::string &name): User(name, Type::Label::label) {}

    static std::shared_ptr<Block> create(const std::string &name, const std::shared_ptr<Function> &function) {
        const auto block = std::make_shared<Block>(name);
        block->set_function(function);
        return block;
    }

    void set_function(const std::shared_ptr<Function> &function) {
        parent = function;
        function->add_block(std::static_pointer_cast<Block>(shared_from_this()));
    }

    [[nodiscard]] bool is_deleted() const { return deleted; }

    void set_deleted(const bool flag = true) { deleted = flag; }

    [[nodiscard]] std::shared_ptr<Function> get_function() const { return parent.lock(); }

    [[nodiscard]] std::vector<std::shared_ptr<Instruction>> &get_instructions() { return instructions; }

    void add_instruction(const std::shared_ptr<Instruction> &instruction) { instructions.emplace_back(instruction); }

    [[nodiscard]] std::string to_string() const override;
};
}

#endif

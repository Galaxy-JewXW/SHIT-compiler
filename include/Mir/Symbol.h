#ifndef SYMBOL_H
#define SYMBOL_H
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Init.h"
#include "Type.h"
#include "Value.h"

namespace Mir::Symbol {
class Symbol {
    const std::string name;
    const std::shared_ptr<Type::Type> type;
    const bool is_constant;
    // 变量对应的初始值
    const std::shared_ptr<Init::Init> init_value;
    // 为变量分配的栈空间，表现为一个llvm alloca语句
    const std::shared_ptr<Value> address;

public:
    Symbol(std::string name, const std::shared_ptr<Type::Type> &type, const bool is_constant,
           const std::shared_ptr<Init::Init> &init_value, const std::shared_ptr<Value> &address) :
        name{std::move(name)}, type{type}, is_constant{is_constant}, init_value{init_value}, address{address} {}

    [[nodiscard]] const std::string &get_name() const { return name; }

    [[nodiscard]] const std::shared_ptr<Type::Type> &get_type() const { return type; }

    [[nodiscard]] bool is_constant_symbol() const { return is_constant; }

    [[nodiscard]] const std::shared_ptr<Init::Init> &get_init_value() const { return init_value; }

    [[nodiscard]] const std::shared_ptr<Value> &get_address() const { return address; }

    // [[nodiscard]] const std::string &to_string() const {
    //     std::ostringstream oss;
    //     oss << "Symbol{" << name << ", " << type->to_string() << ", " << is_constant << ", " << init_value->to_string()
    //         << ", " << address->to_string() << "}";
    //     return oss.str();
    // }
};

class Table {
    std::vector<std::unordered_map<std::string, std::shared_ptr<Symbol>>> symbols; // 修改：使用 std::vector 代替 std::stack

public:
    explicit Table() = default;

    void push_scope();

    void pop_scope();

    void insert_symbol(const std::string &name, const std::shared_ptr<Type::Type> &type, bool is_constant,
                       const std::shared_ptr<Init::Init> &
                       init_value, const std::shared_ptr<Value> &address);

    [[nodiscard]] std::shared_ptr<Symbol> lookup_in_top_scope(const std::string &name) const;

    [[nodiscard]] std::shared_ptr<Symbol> lookup_in_all_scopes(const std::string &name) const;
};
}

#endif

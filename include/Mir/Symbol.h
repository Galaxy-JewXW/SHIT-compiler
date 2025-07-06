#ifndef SYMBOL_H
#define SYMBOL_H

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Type.h"
#include "Value.h"

namespace Mir::Init {
class Init;
}

namespace Mir::Symbol {
class Symbol {
    const std::string name;
    const std::shared_ptr<Type::Type> type;
    const bool is_constant;
    bool is_modified;
    // 变量对应的初始值
    const std::shared_ptr<Init::Init> init_value;
    // 为变量分配的栈空间，表现为一个llvm alloca语句
    const std::shared_ptr<Value> address;

public:
    Symbol(std::string name, const std::shared_ptr<Type::Type> &type, const std::shared_ptr<Init::Init> &init_value,
           const std::shared_ptr<Value> &address, const bool is_constant, const bool is_modified = true) :
        name{std::move(name)}, type{type}, is_constant{is_constant}, is_modified{is_modified}, init_value{init_value},
        address{address} {}

    [[nodiscard]] const std::string &get_name() const { return name; }

    [[nodiscard]] const std::shared_ptr<Type::Type> &get_type() const { return type; }

    [[nodiscard]] bool is_constant_symbol() const { return is_constant; }

    [[nodiscard]] bool is_modified_symbol() const { return is_modified; }

    void set_modified(const bool modified = true) { is_modified = modified; }

    [[nodiscard]] const std::shared_ptr<Init::Init> &get_init_value() const { return init_value; }

    [[nodiscard]] const std::shared_ptr<Value> &get_address() const { return address; }

    // [[nodiscard]] const std::string &to_string() const {
    //     std::ostringstream oss;
    //     oss << "Symbol{" << name << ", " << type->to_string() << ", " << is_constant << ", " <<
    //     init_value->to_string()
    //         << ", " << address->to_string() << "}";
    //     return oss.str();
    // }
};

class Table {
    std::vector<std::unordered_map<std::string, std::shared_ptr<Symbol>>> symbols;

public:
    explicit Table() = default;

    void push_scope();

    void pop_scope();

    void insert_symbol(const std::string &name, const std::shared_ptr<Type::Type> &type,
                       const std::shared_ptr<Init::Init> &init_value, const std::shared_ptr<Value> &address,
                       bool is_constant = false, bool is_modified = true);

    [[nodiscard]] std::shared_ptr<Symbol> lookup_in_current_scope(const std::string &name) const;

    [[nodiscard]] std::shared_ptr<Symbol> lookup_in_all_scopes(const std::string &name) const;
};
} // namespace Mir::Symbol

#endif

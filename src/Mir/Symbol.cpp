#include "Mir/Symbol.h"
#include "Utils/Log.h"

namespace Mir::Symbol {
void Table::push_scope() { symbols.emplace_back(); }

void Table::pop_scope() {
    if (symbols.empty()) {
        log_fatal("Cannot pop when stack is empty.");
    }
    symbols.pop_back();
}

void Table::insert_symbol(const std::string &name, const std::shared_ptr<Type::Type> &type,
                          const std::shared_ptr<Init::Init> &init_value, const std::shared_ptr<Value> &address,
                          const bool is_constant, const bool is_modified, const bool is_function_parameter,
                          const int lineno) {
    // 检查是否可以覆盖已存在的符号
    if (symbols.back().find(name) != symbols.back().end()) {
        if (!can_override_symbol(name)) {
            log_error("Symbol {%s} already exists in current scope at %d.", name.c_str(), lineno);
        }
        // 如果可以覆盖，则直接覆盖
    }
    const auto &symbol =
            std::make_shared<Symbol>(name, type, init_value, address, is_constant, is_modified, is_function_parameter);
    symbols.back()[name] = symbol;
}

bool Table::can_override_symbol(const std::string &name) const {
    // 局部变量可以覆盖函数形参
    return is_function_parameter(name);
}

bool Table::is_function_parameter(const std::string &name) const {
    // 检查符号是否为函数参数，遍历所有作用域
    for (auto it = symbols.rbegin(); it != symbols.rend(); ++it) {
        if (it->find(name) != it->end()) {
            return it->at(name)->is_function_parameter_symbol();
        }
    }
    return false;
}

std::shared_ptr<Symbol> Table::lookup_in_current_scope(const std::string &name) const {
    if (symbols.back().find(name) != symbols.back().end()) {
        return symbols.back().at(name);
    }
    return nullptr;
}

std::shared_ptr<Symbol> Table::lookup_in_all_scopes(const std::string &name) const {
    for (auto it = symbols.rbegin(); it != symbols.rend(); ++it) {
        if (it->find(name) != it->end()) {
            return it->at(name);
        }
    }
    return nullptr;
}
} // namespace Mir::Symbol

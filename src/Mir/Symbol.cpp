#include "Mir/Symbol.h"
#include "Utils/Log.h"

namespace Mir::Symbol {
void Table::push_scope() {
    symbols.emplace_back();
}

void Table::pop_scope() {
    if (symbols.empty()) {
        log_fatal("Cannot pop when stack is empty.");
    }
    symbols.pop_back();
}

void Table::insert_symbol(const std::string &name, const std::shared_ptr<Type::Type> &type,
                          const std::shared_ptr<Init::Init> &init_value, const std::shared_ptr<Value> &address,
                          const bool is_constant, const bool is_modified) {
    if (symbols.back().find(name) != symbols.back().end()) {
        log_error("Symbol {%s} already exists in current scope.", name.c_str());
    }
    const auto &symbol = std::make_shared<Symbol>(name, type, init_value, address, is_constant, is_modified);
    symbols.back()[name] = symbol;
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
}

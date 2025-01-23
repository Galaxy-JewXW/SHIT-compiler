#include "Mir/Symbol.h"

#include <memory>
#include <stdexcept>




    std::shared_ptr<Value> SymbolTable::getSymbol(const std::string &name) {
        const auto it = _values.find(name);
        if (it == _values.end()) {
            if (this->parent()) {
                return this->parent()->getSymbol(name);
            }
            return nullptr;
        }
        return it->second;
    }

    std::shared_ptr<Value> SymbolTable::getSymbolAlone(const std::string &name) {
        const auto it = _values.find(name);
        if (it == _values.end()) {
            return nullptr;
        }
        return it->second;
    }

    void SymbolTable::addSymbol(const std::string &name, const std::shared_ptr<Value>& value) {
        if (const auto it = _values.find(name); it == _values.end()) {
            _values[name] = value;
        }
        else {
            throw std::runtime_error("SymbolTable::addSymbol: Symbol already exists");
            // 只要源程序保证正确、正常解析，就不会出现重定义语义错误
        }
    }


#ifndef SYMBOL_H
#define SYMBOL_H
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Value.h"



    class SymbolTable {
        std::weak_ptr<SymbolTable> _parent;
        std::unordered_map<std::string, std::shared_ptr<Value>> _values;

    public:
            explicit SymbolTable(const std::shared_ptr<SymbolTable>& parent) : _parent(parent), _values() {

        }

            SymbolTable(const SymbolTable&) = delete;
            SymbolTable& operator=(const SymbolTable&) = delete;

            SymbolTable(SymbolTable&&) = default;
            SymbolTable& operator=(SymbolTable&&) = default;

        std::shared_ptr<SymbolTable> parent() const { return _parent.lock();}

        void addSymbol(const std::string& name, const std::shared_ptr<Value>& value);
        std::shared_ptr<Value> getSymbol(const std::string& name);
        std::shared_ptr<Value> getSymbolAlone(const std::string& name);
    };




#endif //SYMBOL_H

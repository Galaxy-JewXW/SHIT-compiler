package middle.symbol;

import exceptions.SemanticException;

import java.util.HashMap;

public class SymbolTable {
    private final HashMap<String, Symbol> symbols = new HashMap<>();
    private final SymbolTable parent;

    public SymbolTable(SymbolTable parent) {
        this.parent = parent;
    }

    public SymbolTable getParent() {
        return parent;
    }

    // 在本级寻找符号的方法
    public Symbol get(String name) {
        return symbols.getOrDefault(name, null);
    }

    public void put(Symbol symbol) {
        if (symbols.containsKey(symbol.getName())) {
            throw new SemanticException("Symbol " + symbol.getName() + " already exists");
        }
        symbols.put(symbol.getName(), symbol);
    }

    // 逐级向上寻找符号的方法
    public Symbol search(String name) {
        SymbolTable t = this;
        while (t != null) {
            if (t.get(name) != null) {
                return t.get(name);
            }
            t = t.parent;
        }
        return null;
    }
}

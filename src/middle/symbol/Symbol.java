package middle.symbol;

import middle.InitVal;
import middle.IrType;
import middle.llvm.Value;

public class Symbol {
    private final String name;
    private final IrType type;
    private final boolean isConstant;
    // 变量对应的初始值
    private final InitVal initVal;
    // 变量对应的栈空间，表现为一个llvm alloc语句
    private final Value address;

    public Symbol(String name, IrType type, boolean isConstant, InitVal initVal, Value address) {
        this.name = name;
        this.type = type;
        this.isConstant = isConstant;
        this.initVal = initVal;
        this.address = address;
    }

    public String getName() {
        return name;
    }

    public IrType getType() {
        return type;
    }

    public boolean isConstant() {
        return isConstant;
    }

    public InitVal getInitVal() {
        return initVal;
    }

    public Value getAddress() {
        return address;
    }

    @Override
    public String toString() {
        return "Symbol{" +
                "name='" + name + '\'' +
                ", type=" + type +
                ", isConstant=" + isConstant +
                '}';
    }
}

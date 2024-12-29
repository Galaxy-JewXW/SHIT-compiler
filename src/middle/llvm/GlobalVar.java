package middle.llvm;

import middle.InitVal;
import middle.IrType;
import middle.IrType.PointerType;

public class GlobalVar extends Value {
    private final String label;
    private final InitVal initVal;
    private final boolean isConstant;

    public GlobalVar(IrType type, String label, InitVal initVal, boolean isConstant) {
        super(label, new PointerType(type));
        this.label = "@gv_" + label + "_";
        this.initVal = initVal;
        this.isConstant = isConstant;
    }

    public String getLabel() {
        return label;
    }

    @Override
    public String toString() {
        return getLabel() + " = dso_local"
                + (isConstant ? " constant " : " global ") +
                initVal;
    }
}

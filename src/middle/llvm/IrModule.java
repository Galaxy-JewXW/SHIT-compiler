package middle.llvm;

import middle.IrType;

import java.util.ArrayList;
import java.util.stream.Collectors;

public class IrModule extends User {
    private final ArrayList<GlobalVar> globalVars = new ArrayList<>();
    private final ArrayList<Function> functions = new ArrayList<>();

    private static final IrModule instance = new IrModule();
    public static IrModule getInstance() {
        return instance;
    }

    private IrModule() {
        super("__module__", IrType.LabelType.LABEL);
    }

    public void addGlobalVar(GlobalVar gv) {
        globalVars.add(gv);
    }

    public void addFunction(Function f) {
        functions.add(f);
    }

    @Override
    public String toString() {
        return globalVars.stream().map(GlobalVar::toString)
                        .collect(Collectors.joining("\n")) + "\n\n" +
                functions.stream().map(Function::toString)
                        .collect(Collectors.joining("\n\n"));
    }

}

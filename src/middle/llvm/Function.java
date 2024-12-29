package middle.llvm;

import middle.IrType;

import java.util.ArrayList;
import java.util.stream.Collectors;

public class Function extends User {
    // 函数返回值
    private final IrType returnType;
    // 形式参数列表
    private final ArrayList<FuncArgument> arguments = new ArrayList<>();
    // 基本块列表
    private final ArrayList<BasicBlock> blocks = new ArrayList<>();

    public Function(IrType returnType, String name) {
        super(name, IrType.LabelType.LABEL);
        this.returnType = returnType;
    }

    public IrType getReturnType() {
        return returnType;
    }

    public void addArgument(FuncArgument arg) {
        arguments.add(arg);
    }

    public FuncArgument getArgument(int index) {
        return arguments.get(index);
    }

    public void addBasicBlock(BasicBlock block) {
        blocks.add(block);
    }

    @Override
    public String toString() {
        String paramInfo = arguments.stream().map(Object::toString)
                .collect(Collectors.joining(", "));
        return "define dso_local " + returnType + " " +
                getName() + "(" + paramInfo + ") {\n" +
                blocks.stream().map(Object::toString)
                        .collect(Collectors.joining("\n")) +
                "\n}";
    }

    public static class FuncArgument extends Value {
        private int index;
        public FuncArgument(IrType type, int index) {
            super(IrData.getVarName(), type);
            this.index = index;
        }

        @Override
        public String toString() {
            return getType() + " " + getName();
        }
    }
}

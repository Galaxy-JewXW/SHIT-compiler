package middle.llvm;

import exceptions.SemanticException;
import middle.IrType;
import middle.IrType.VoidType;

import java.util.ArrayList;
import java.util.stream.Collectors;

public abstract class Instruction extends User {

    private final OperatorType op;
    private BasicBlock parentBlock;

    public Instruction(IrType irType, OperatorType op, BasicBlock block, String name) {
        super(name, irType);
        this.op = op;
        this.parentBlock = block;
        parentBlock.addInstruction(this);
    }

    public Instruction(IrType irType, OperatorType op, BasicBlock block) {
        super(IrData.getVarName(), irType);
        this.op = op;
        this.parentBlock = block;
        parentBlock.addInstruction(this);
    }

    public enum OperatorType {
        ADD("add"), // +
        SUB("sub"), // -
        MUL("mul"), // *
        SDIV("sdiv"), // /
        SREM("srem"), // %
        // 整数类比较
        ICMP_EQ("icmp eq"), // ==
        ICMP_NE("icmp ne"), // !=
        ICMP_SGT("icmp sgt"), // >
        ICMP_SGE("icmp sge"), // >=
        ICMP_SLT("icmp slt"), // <
        ICMP_SLE("icmp sle"), // <=
        // 浮点数类比较
        FCMP_EQ("fcmp oeq"),
        FCMP_NE("fcmp one"),
        FCMP_OGT("fcmp ogt"),
        FCMP_OGE("fcmp oge"),
        FCMP_OLT("fcmp olt"),
        FCMP_OLE("fcmp ole"),
        // 其他类型转换
        ZEXT("zext"),
        FPTOSI("fptosi"),
        SITOFP("sitofp"),
        CALL("call"),
        // 这里把io指令作为call的一种特例提出来看待
        IO("io"),
        ALLOC("alloca"),
        STORE("store"),
        LOAD("load"),
        GEP("getelementptr"),
        PHI("phi"),
        BR("br"),
        RET("ret"),
        MOVE("move");

        private final String name;

        OperatorType(String name) {
            this.name = name;
        }

        @Override
        public String toString() {
            return name;
        }
    }

    public static class Alloca extends Instruction {
        // alloc指令在栈上分配一个地址空间，返回值是一个指针
        private final IrType targetType;

        public Alloca(IrType targetType, BasicBlock parentBlock) {
            super(new IrType.PointerType(targetType), OperatorType.ALLOC, parentBlock);
            this.targetType = targetType;
        }

        public IrType getTargetType() {
            return targetType;
        }

        @Override
        public String toString() {
            return getName() + " = alloca " + targetType;
        }
    }

    public static class Store extends Instruction {
        public Store(Value addr, Value value, BasicBlock parentBlock) {
            super(IrType.VoidType.VOID, OperatorType.STORE, parentBlock, "");
            addOperand(addr);
            addOperand(value);
            assert addr.getType() instanceof IrType.PointerType;
            assert value.getType().equals(((IrType.PointerType) addr.getType()).getTargetType());
        }

        public Value getAddr() {
            return getOperands().get(0);
        }

        public Value getValue() {
            return getOperands().get(1);
        }

        @Override
        public String toString() {
            return "store "
                    + getValue().getType() + " "
                    + getValue().getName() + ", " +
                    getAddr().getType() + " "
                    + getAddr().getName();
        }
    }

    public static class Load extends Instruction {
        public Load(Value addr, BasicBlock parentBlock) {
            super(((IrType.PointerType) addr.getType()).getTargetType(), OperatorType.LOAD, parentBlock);
            addOperand(addr);
        }

        public Value getAddr() {
            return getOperands().get(0);
        }

        @Override
        public String toString() {
            return getName() + " = load " + getType() + ", "
                    + getAddr().getType() + " " + getAddr().getName();
        }
    }

    public static class GetElementPtr extends Instruction {
        public GetElementPtr(Value addr, Value index, BasicBlock parentBlock) {
            super(calcType(addr), OperatorType.GEP, parentBlock);
            assert index.getType().isI32Type();
            addOperand(addr);
            addOperand(index);
        }

        public static IrType calcType(Value addr) {
            IrType type = addr.getType();
            if (!(type instanceof IrType.PointerType pointerType)) {
                throw new SemanticException("First operand must be a pointer type");
            }
            IrType targetType = pointerType.getTargetType();
            if (targetType instanceof IrType.ArrayType arrayType) {
                return new IrType.PointerType(arrayType.getElementType());
            } else if (targetType.isI32Type() || targetType.isFloatType()) {
                return new IrType.PointerType(targetType);
            }
            throw new SemanticException("First operand must be a pointer type");
        }

        public Value getAddr() {
            return getOperands().get(0);
        }

        public Value getIndex() {
            return getOperands().get(1);
        }

        @Override
        public String toString() {
            Value addr = getAddr();
            IrType.PointerType pointerType = (IrType.PointerType) addr.getType();
            IrType targetType = pointerType.getTargetType();

            StringBuilder sb = new StringBuilder(getName())
                    .append(" = getelementptr inbounds ")
                    .append(targetType)
                    .append(", ")
                    .append(pointerType)
                    .append(" ")
                    .append(addr.getName());
            if (targetType instanceof IrType.ArrayType) {
                sb.append(", i32 0, i32 ");
            } else {
                sb.append(", i32 ");
            }
            sb.append(getIndex().getName());
            return sb.toString();
        }
    }

    public static class Call extends Instruction {
        private Call(Function calledFunction, ArrayList<Value> parameters, BasicBlock parentBlock) {
            super(calledFunction.getReturnType(), OperatorType.CALL, parentBlock, "");
            addOperand(calledFunction);
            for (Value param : parameters) {
                addOperand(param);
            }
            if (!calledFunction.getReturnType().equals(VoidType.VOID)) {
                setName(IrData.getVarName());
            }
        }

        public Function getCalledFunction() {
            return (Function) getOperands().get(0);
        }

        public ArrayList<Value> getParameters() {
            return new ArrayList<>(getOperands().subList(1, getOperands().size()));
        }

        public String getCallee() {
            String paramInfo = getParameters().stream()
                    .map(param -> param.getType() + " " + param.getName())
                    .collect(Collectors.joining(", "));
            return String.format("%s(%s)", getCalledFunction().getName(), paramInfo);
        }

        @Override
        public String toString() {
            String functionCall = getCallee();
            return getType().equals(VoidType.VOID)
                    ? String.format("call void %s", functionCall)
                    : String.format("%s = call %s %s", getName(), getType(), functionCall);
        }
    }

    public static class Fptosi extends Instruction {
        public Fptosi(Value value, IrType targetType, BasicBlock parentBlock) {
            super(targetType, OperatorType.FPTOSI, parentBlock);
            assert targetType.isI32Type();
            assert value.getType().isFloatType();
            addOperand(value);
        }

        public Value getOriginValue() {
            return getOperands().get(0);
        }

        @Override
        public String toString() {
            return getName() + " = fptosi " + getOriginValue().getType() + " "
                    + getOriginValue().getName() + " to " +
                    getType();
        }
    }

    public static class Sitofp extends Instruction {
        public Sitofp(Value value, IrType targetType, BasicBlock parentBlock) {
            super(targetType, OperatorType.SITOFP, parentBlock);
            assert targetType.isFloatType();
            assert value.getType().isI32Type();
            addOperand(value);
        }

        public Value getOriginValue() {
            return getOperands().get(0);
        }

        @Override
        public String toString() {
            return getName() + " = sitofp " + getOriginValue().getType() + " "
                    + getOriginValue().getName() + " to " +
                    getType();
        }
    }

    public static class Icmp extends Instruction {
        private final OperatorType opType;

        public Icmp(Value operand1, OperatorType opType, Value operand2, BasicBlock parentBlock) {
            super(IrType.IntegerType.i1, opType, parentBlock);
            assert operand1.getType().isI32Type();
            assert operand2.getType().isI32Type();
            addOperand(operand1);
            addOperand(operand2);
            this.opType = opType;
        }

        public Value getOperand1() {
            return getOperands().get(0);
        }

        public Value getOperand2() {
            return getOperands().get(1);
        }

        @Override
        public String toString() {
            return getName() + " = " + opType + " i32 " + getOperand1().getName()
                    + ", " + getOperand2().getName();
        }
    }

    public static class Fcmp extends Instruction {
        private final OperatorType opType;

        public Fcmp(Value operand1, OperatorType opType, Value operand2, BasicBlock parentBlock) {
            super(IrType.IntegerType.i1, opType, parentBlock);
            assert operand1.getType().isFloatType();
            assert operand2.getType().isFloatType();
            addOperand(operand1);
            addOperand(operand2);
            this.opType = opType;
        }

        public Value getOperand1() {
            return getOperands().get(0);
        }

        public Value getOperand2() {
            return getOperands().get(1);
        }

        @Override
        public String toString() {
            return getName() + " = " + opType + " float " + getOperand1().getName()
                    + ", " + getOperand2().getName();
        }
    }

    public static class Zext extends Instruction {
        public Zext(Value value, IrType targetType, BasicBlock parentBlock) {
            super(targetType, OperatorType.ZEXT, parentBlock);
            assert value.getType().isI1Type() && targetType.isI32Type();
            addOperand(value);
        }

        public Value getOriginValue() {
            return getOperands().get(0);
        }

        @Override
        public String toString() {
            return getName() + " = zext " + getOriginValue().getType() + " "
                    + getOriginValue().getName() + " to " +
                    getType();
        }
    }
}

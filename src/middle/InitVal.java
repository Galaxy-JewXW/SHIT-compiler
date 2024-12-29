package middle;

import antlr.SysY2022Parser;
import exceptions.SemanticException;
import middle.llvm.ConstantValue;
import middle.llvm.Value;
import middle.symbol.SymbolTable;
import utils.Calculator;

import java.util.ArrayList;

// 记录每个变量symbol拥有的初始值
public abstract class InitVal {
    private final IrType type;

    public InitVal(IrType type) {
        this.type = type;
    }

    public IrType getType() {
        return type;
    }

    public static class ValueInitVal extends InitVal {
        private final Value value;

        public ValueInitVal(Value value, IrType type) {
            super(type);
            this.value = value;
        }

        public static ValueInitVal parseValueInitVal(IrType irType, SysY2022Parser.ConstExpContext constExpContext,
                                                     SymbolTable symbolTable) {
            Object curValue = Calculator.evalExp(constExpContext.addExp(), symbolTable);
            if (irType.isI32Type()) {
                if (curValue instanceof Float f) {
                    return new ValueInitVal(new ConstantValue.ConstantInt(f.intValue()), IrType.IntegerType.i32);
                } else if (curValue instanceof Integer i) {
                    return new ValueInitVal(new ConstantValue.ConstantInt(i), IrType.IntegerType.i32);
                }
            } else if (irType.isFloatType()) {
                if (curValue instanceof Float f) {
                    return new ValueInitVal(new ConstantValue.ConstantFloat(f), IrType.FloatType.f32);
                } else if (curValue instanceof Integer i) {
                    return new ValueInitVal(new ConstantValue.ConstantFloat((float) i), IrType.FloatType.f32);
                }
            }
            throw new SemanticException("Illegal constant type: " + irType);
        }

        public static ValueInitVal parseZeroValueInitVal(IrType irType) {
            if (irType.isI32Type()) {
                return new ValueInitVal(new ConstantValue.ConstantInt(0), IrType.IntegerType.i32);
            } else if (irType.isFloatType()) {
                return new ValueInitVal(new ConstantValue.ConstantFloat(0.0f), IrType.FloatType.f32);
            }
            throw new SemanticException("Illegal constant type: " + irType);
        }

        public Value getValue() {
            return value;
        }

        @Override
        public String toString() {
            return getType().toString() + " " + value.getName();
        }
    }

    public static class ArrayInitVal extends InitVal {
        // arrayInitVal是多个initVal的集合，可用于一维或多维数组
        private final ArrayList<InitVal> initVals = new ArrayList<>();
        private final ArrayList<ValueInitVal> flattenedInitVals;

        public ArrayInitVal(IrType irType, ArrayList<ValueInitVal> flattenedInitVals) {
            super(irType);
            this.flattenedInitVals = new ArrayList<>(flattenedInitVals);
        }

        public ArrayList<ValueInitVal> getFlattenedInitVals() {
            return new ArrayList<>(flattenedInitVals);
        }

        public static ArrayInitVal parseArrayInitVal(
                IrType irType, SysY2022Parser.ConstInitValContext ctx, SymbolTable symbolTable) {
            if (!(irType instanceof IrType.ArrayType arrayType)) {
                throw new SemanticException(irType.toString() + " is not an array type");
            }
            ArrayList<ValueInitVal> flattened = flattenArrayInitVal(irType, ctx, symbolTable, 0);
            return foldArrayInitVal(irType, flattened);
        }

        private static ArrayInitVal foldArrayInitVal(IrType irType, ArrayList<ValueInitVal> flattened) {
            if (!(irType instanceof IrType.ArrayType arrayType)) {
                throw new SemanticException(irType.toString() + " is not an array type");
            }
            ArrayInitVal arrayInitVal = new ArrayInitVal(irType, flattened);
            int dimLength = arrayType.getElementCount();
            int length = flattened.size() / dimLength;
            if (length == 1) {
                for (int i = 0; i < dimLength; i++) {
                    arrayInitVal.addInitVal(flattened.get(i));
                }
            } else {
                IrType elementType = arrayType.getElementType();
                for (int i = 0; i < dimLength; i++) {
                    arrayInitVal.addInitVal(foldArrayInitVal(elementType,
                            new ArrayList<>(flattened.subList(length * i, length * (i + 1)))));
                }
            }
            return arrayInitVal;
        }

        private static ArrayList<ValueInitVal> flattenArrayInitVal(
                IrType irType, SysY2022Parser.ConstInitValContext ctx, SymbolTable symbolTable, int depth) {
            if (!(irType instanceof IrType.ArrayType arrayType)) {
                throw new SemanticException(irType.toString() + " is not an array type");
            }
            int curDimLength = arrayType.getElementCount();
            IrType elementType = arrayType.getElementType();
            int elementSize = elementType.isArrayType()
                    ? ((IrType.ArrayType) elementType).getFlattenedElementCount()
                    : 1;
            ArrayList<ValueInitVal> flattenedInitVals = new ArrayList<>();
            IrType atomicType = arrayType.getAtomicType();
            for (SysY2022Parser.ConstInitValContext constInitValContext : ctx.constInitVal()) {
                if (constInitValContext.constExp() != null) {
                    flattenedInitVals.add(ValueInitVal.parseValueInitVal(
                            atomicType, constInitValContext.constExp(), symbolTable));
                } else {
                    int pos = flattenedInitVals.size();
                    for (int i = 0; i < (elementSize - (pos % elementSize)) % elementSize; i++) {
                        flattenedInitVals.add(ValueInitVal.parseZeroValueInitVal(atomicType));
                    }
                    ArrayList<ValueInitVal> subArrayInitVals = flattenArrayInitVal(
                            arrayType.getElementType(), constInitValContext, symbolTable, depth + 1);
                    flattenedInitVals.addAll(subArrayInitVals);
                }
            }
            for (int i = flattenedInitVals.size(); i < curDimLength * elementSize; i++) {
                flattenedInitVals.add(ValueInitVal.parseZeroValueInitVal(atomicType));
            }
            return flattenedInitVals;
        }

        public void addInitVal(InitVal initVal) {
            initVals.add(initVal);
        }

        public int size() {
            return initVals.size();
        }

        public InitVal getInitVal(int index) {
            return initVals.get(index);
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder(getType().toString() + " [");
            for (int i = 0; i < initVals.size(); i++) {
                sb.append(initVals.get(i).toString());
                if (i < (initVals.size() - 1)) {
                    sb.append(", ");
                }
            }
            sb.append("]");
            return sb.toString();
        }
    }


}

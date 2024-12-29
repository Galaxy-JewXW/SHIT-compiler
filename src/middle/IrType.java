package middle;

import exceptions.SemanticException;

public abstract class IrType {
    public boolean isArrayType() {
        return false;
    }

    public boolean isIntegerType() {
        return false;
    }

    public boolean isI32Type() {
        return false;
    }

    public boolean isI1Type() {
        return false;
    }

    public boolean isFloatType() {
        return false;
    }

    public boolean isPointerType() {
        return false;
    }

    public boolean isVoidType() {
        return false;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof IrType irType) {
            return irType.toString().equals(this.toString());
        }
        return false;
    }

    @Override
    public int hashCode() {
        return this.toString().hashCode();
    }

    public static class ArrayType extends IrType {
        private final int elementCount;
        private final IrType elementType;

        public ArrayType(int elementCount, IrType elementType) {
            this.elementCount = elementCount;
            this.elementType = elementType;
        }

        public int getElementCount() {
            return elementCount;
        }

        public IrType getElementType() {
            return elementType;
        }

        // 获取最小元素的类型，'int'或'float'
        public IrType getAtomicType() {
            IrType current = elementType;
            while (current.isArrayType()) {
                current = ((ArrayType) current).getElementType();
            }
            return current;
        }

        public int getFlattenedElementCount() {
            if (elementType.isIntegerType() || elementType.isFloatType()) {
                return elementCount;
            } else if (elementType.isArrayType()) {
                return ((ArrayType) elementType).getFlattenedElementCount() * elementCount;
            }
            throw new SemanticException("Unsupported element type: " + elementType);
        }

        @Override
        public boolean isArrayType() {
            return true;
        }

        @Override
        public String toString() {
            return "[" + elementCount + " x " + elementType + "]";
        }
    }

    public static class FloatType extends IrType {
        public static final FloatType f32 = new FloatType();

        private FloatType() {
        }

        @Override
        public boolean isFloatType() {
            return true;
        }

        @Override
        public String toString() {
            return "float";
        }
    }

    public static class IntegerType extends IrType {
        public static final IntegerType i1 = new IntegerType(1);
        public static final IntegerType i32 = new IntegerType(32);
        private final int bits;

        private IntegerType(int bits) {
            this.bits = bits;
        }

        @Override
        public String toString() {
            return "i" + bits;
        }

        @Override
        public boolean isIntegerType() {
            return true;
        }

        @Override
        public boolean isI1Type() {
            return bits == 1;
        }

        @Override
        public boolean isI32Type() {
            return bits == 32;
        }
    }

    public static class PointerType extends IrType {
        private final IrType targetType;

        public PointerType(IrType targetType) {
            this.targetType = targetType;
        }

        public IrType getTargetType() {
            return targetType;
        }

        @Override
        public boolean isPointerType() {
            return true;
        }

        @Override
        public String toString() {
            return targetType.toString() + "*";
        }
    }

    public static class VoidType extends IrType {
        public static final VoidType VOID = new VoidType();

        private VoidType() {
        }

        @Override
        public boolean isVoidType() {
            return true;
        }

        @Override
        public String toString() {
            return "void";
        }
    }

    // 占位类型
    public static class LabelType extends IrType {
        public static final LabelType LABEL = new LabelType();

        private LabelType() {
        }

        @Override
        public String toString() {
            return "label";
        }
    }
}

package middle.llvm;

import middle.IrType;
import middle.IrType.FloatType;
import middle.IrType.IntegerType;

public abstract class ConstantValue extends Value {
    public ConstantValue(String name, IrType type) {
        super(name, type);
    }

    public abstract boolean isZero();

    public abstract Object getConstantValue();

    public static class ConstantFloat extends ConstantValue {
        private final float constFloatValue;

        public ConstantFloat(float constFloatValue) {
            super("0x" + Double.doubleToRawLongBits(constFloatValue), FloatType.f32);
            this.constFloatValue = constFloatValue;
        }

        @Override
        public boolean isZero() {
            return constFloatValue == 0.0f;
        }

        @Override
        public Object getConstantValue() {
            return constFloatValue;
        }
    }

    public static class ConstantInt extends ConstantValue {
        private final int constIntValue;

        public ConstantInt(int constIntValue) {
            super(Integer.toString(constIntValue), IntegerType.i32);
            this.constIntValue = constIntValue;
        }

        @Override
        public boolean isZero() {
            return constIntValue == 0;
        }

        @Override
        public Object getConstantValue() {
            return constIntValue;
        }
    }

    public static class ConstantBool extends ConstantValue {
        private static final ConstantBool CONSTANT_BOOL_TRUE = new ConstantBool(1);
        private static final ConstantBool CONSTANT_BOOL_FALSE = new ConstantBool(0);
        private final int constBoolValue;

        private ConstantBool(int constBoolValue) {
            super(Integer.toString(constBoolValue), IntegerType.i1);
            this.constBoolValue = constBoolValue;
        }

        @Override
        public boolean isZero() {
            return constBoolValue == 0;
        }

        @Override
        public Object getConstantValue() {
            return constBoolValue;
        }

        public static ConstantBool getConstantBool(boolean value) {
            return value ? CONSTANT_BOOL_TRUE : CONSTANT_BOOL_FALSE;
        }

        public static ConstantBool getConstantBool(int value) {
            return value == 0 ? CONSTANT_BOOL_FALSE : CONSTANT_BOOL_TRUE;
        }

        public static ConstantBool getConstantBool(float value) {
            return value == 0.0f ? CONSTANT_BOOL_FALSE : CONSTANT_BOOL_TRUE;
        }
    }
}

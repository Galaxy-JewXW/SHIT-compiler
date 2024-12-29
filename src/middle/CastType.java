package middle;

import exceptions.SemanticException;
import middle.llvm.BasicBlock;
import middle.llvm.ConstantValue;
import middle.llvm.Instruction;
import middle.llvm.Value;

public class CastType {
    public static Value cast(Value v, IrType targetType, BasicBlock curBlock) {
        if (!(v.getType().isIntegerType() || v.getType().isFloatType())) {
            throw new SemanticException(v.getType() + " cannot be cast to " + targetType);
        }
        if (!(targetType.isIntegerType() || targetType.isFloatType())) {
            throw new SemanticException(v.getType() + " cannot be cast to " + targetType);
        }
        if (v instanceof ConstantValue constantValue) {
            return castConstantValue(constantValue, targetType);
        }
        if (v.getType().equals(targetType)) {
            return v;
        }
        if (v.getType().isI1Type()) {
            if (targetType.isI32Type()) {
                return new Instruction.Zext(v, IrType.IntegerType.i32, curBlock);
            } else if (targetType.isFloatType()) {
                return new Instruction.Sitofp(new Instruction.Zext(v, IrType.IntegerType.i32, curBlock),
                        IrType.FloatType.f32, curBlock);
            }
        }
        if (v.getType().isI32Type()) {
            if (targetType.isI1Type()) {
                return new Instruction.Icmp(v, Instruction.OperatorType.ICMP_NE,
                        new ConstantValue.ConstantInt(0), curBlock);
            } else if (targetType.isFloatType()) {
                return new Instruction.Sitofp(v, IrType.FloatType.f32, curBlock);
            }
        }
        if (v.getType().isFloatType()) {
            if (targetType.isI32Type()) {
                return new Instruction.Fptosi(v, IrType.IntegerType.i32, curBlock);
            } else if (targetType.isI1Type()) {
                return new Instruction.Fcmp(v, Instruction.OperatorType.FCMP_NE,
                        new ConstantValue.ConstantFloat(0.0f), curBlock);
            }
        }
        throw new SemanticException(v.getType() + " cannot be cast to " + targetType);
    }

    private static ConstantValue castConstantValue(ConstantValue v, IrType targetType) {
        if (v.getType().equals(targetType)) {
            return v;
        }
        if (v.getType().isI1Type()) {
            if (targetType.isI32Type()) {
                return new ConstantValue.ConstantInt((int) v.getConstantValue());
            } else if (targetType.isFloatType()) {
                return new ConstantValue.ConstantFloat((float) ((int) v.getConstantValue()));
            }
        }
        if (v.getType().isI32Type()) {
            if (targetType.isI1Type()) {
                return ConstantValue.ConstantBool.getConstantBool((int) v.getConstantValue());
            } else if (targetType.isFloatType()) {
                return new ConstantValue.ConstantFloat((float) ((int) v.getConstantValue()));
            }
        }
        if (v.getType().isFloatType()) {
            if (targetType.isI32Type()) {
                return new ConstantValue.ConstantInt((int) ((float) v.getConstantValue()));
            } else if (targetType.isI1Type()) {
                return ConstantValue.ConstantBool.getConstantBool((float) v.getConstantValue());
            }
        }
        throw new SemanticException(v.getType() + " cannot be cast to " + targetType);
    }
}

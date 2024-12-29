package utils;

import antlr.SysY2022Parser;
import exceptions.SemanticException;
import middle.InitVal;
import middle.InitVal.ArrayInitVal;
import middle.InitVal.ValueInitVal;
import middle.llvm.ConstantValue;
import middle.llvm.Value;
import middle.symbol.Symbol;
import middle.symbol.SymbolTable;
import org.antlr.v4.runtime.tree.ParseTree;

import java.util.ArrayList;

public class Calculator {
    private static SymbolTable symbolTable = null;

    public static Object evalExp(SysY2022Parser.AddExpContext ctx, SymbolTable symbolTable) {
        Calculator.symbolTable = symbolTable;
        Object result = evalMulExp(ctx.mulExp(0));
        int l = ctx.mulExp().size();
        for (int i = 1; i < l; i++) {
            Object append = evalMulExp(ctx.mulExp(i));
            ParseTree op = ctx.getChild(2 * i - 1);
            result = eval(result, append, op.getText());
        }
        Calculator.symbolTable = null;
        return result;
    }

    private static Object evalAddExp(SysY2022Parser.AddExpContext ctx) {
        Object result = evalMulExp(ctx.mulExp(0));
        int l = ctx.mulExp().size();
        for (int i = 1; i < l; i++) {
            Object append = evalMulExp(ctx.mulExp(i));
            ParseTree op = ctx.getChild(2 * i - 1);
            result = eval(result, append, op.getText());
        }
        return result;
    }

    private static Object evalMulExp(SysY2022Parser.MulExpContext ctx) {
        Object result = evalUnaryExp(ctx.unaryExp(0));
        int l = ctx.unaryExp().size();
        for (int i = 1; i < l; i++) {
            Object append = evalUnaryExp(ctx.unaryExp(i));
            ParseTree op = ctx.getChild(2 * i - 1);
            result = eval(result, append, op.getText());
        }
        return result;
    }

    private static Object evalUnaryExp(SysY2022Parser.UnaryExpContext ctx) {
        if (ctx.IDENT() != null) {
            throw new SemanticException("ConstExp calculation cannot have function call");
        }
        if (ctx.primaryExp() != null) {
            return evalPrimaryExp(ctx.primaryExp());
        }
        if (ctx.unaryOp() != null && ctx.unaryExp() != null) {
            Object result = evalUnaryExp(ctx.unaryExp());
            SysY2022Parser.UnaryOpContext op = ctx.unaryOp();
            if (op.ADD() != null) {
                return result;
            }
            if (op.SUB() != null) {
                if (result instanceof Integer) {
                    return -(int) result;
                }
                if (result instanceof Float) {
                    return -(float) result;
                }
                throw new SemanticException("Unrecognized unaryExp result type");
            }
            if (op.NOT() != null) {
                if (result instanceof Integer) {
                    return (int) result == 0 ? 1 : 0;
                }
                if (result instanceof Float) {
                    return (float) result == 0.0f ? 1 : 0;
                }
                throw new SemanticException("Unrecognized unaryExp result type");
            }
            throw new SemanticException("Unrecognized operator type: " + op.getText());
        }
        throw new SemanticException("Unrecognized unaryExp");
    }

    private static Object evalPrimaryExp(SysY2022Parser.PrimaryExpContext ctx) {
        if (ctx.exp() != null) {
            return evalAddExp(ctx.exp().addExp());
        }
        if (ctx.number() != null) {
            return evalNumber(ctx.number());
        }
        if (ctx.lVal() != null) {
            return evalLVal(ctx.lVal());
        }
        throw new SemanticException("Unrecognized primaryExp");
    }

    private static Object evalNumber(SysY2022Parser.NumberContext ctx) {
        if (ctx.FLOAT_NUMBER() != null) {
            return Float.parseFloat(ctx.FLOAT_NUMBER().getText());
        }
        if (ctx.HEX_CONST() != null || ctx.OCT_CONST() != null) {
            return Integer.decode(ctx.HEX_CONST().getText());
        }
        if (ctx.INT_CONST() != null) {
            return Integer.parseInt(ctx.INT_CONST().getText());
        }
        throw new SemanticException("Unrecognized number: " + ctx.getText());
    }

    private static Object evalLVal(SysY2022Parser.LValContext ctx) {
        String identString = ctx.IDENT().getText();
        Symbol varSymbol = symbolTable.search(identString);
        if (varSymbol == null) {
            throw new SemanticException("Ident undefined: " + identString);
        }
        // 编译期只有常量才能计算出值
        if (!varSymbol.isConstant()) {
            throw new SemanticException("Not a constant symbol: " + identString);
        }
        InitVal varInitVal = varSymbol.getInitVal();
        ArrayList<Integer> indexes = new ArrayList<>();
        for (SysY2022Parser.ExpContext exp : ctx.exp()) {
            Object index = Calculator.evalAddExp(exp.addExp());
            if (!(index instanceof Integer)) {
                throw new SemanticException("Not integer index: " + index);
            }
            if ((int) index < 0) {
                throw new SemanticException("Not positive index: " + index);
            }
            indexes.add((int) index);
        }
        for (Integer index : indexes) {
            if (!(varInitVal instanceof ArrayInitVal arrayInitVal)) {
                throw new SemanticException("Not an array: " + identString);
            }
            varInitVal = arrayInitVal.getInitVal(index);
        }
        // 编译期计算一定算得出具体的int或float值
        if (!(varInitVal instanceof ValueInitVal valueInitVal)) {
            throw new SemanticException("Not an variable or constant: " + varInitVal);
        }
        Value value = valueInitVal.getValue();
        if (!(value instanceof ConstantValue constantValue)) {
            throw new SemanticException("Not a constant: " + value);
        }
        return constantValue.getConstantValue();
    }

    private static Object eval(Object left, Object right, String op) {
        if (left instanceof Float || right instanceof Float) {
            float f1 = left instanceof Integer
                    ? ((Integer) left).floatValue()
                    : (float) left;
            float f2 = right instanceof Integer
                    ? ((Integer) right).floatValue()
                    : (float) right;
            return switch (op) {
                case "+" -> f1 + f2;
                case "-" -> f1 - f2;
                case "*" -> f1 * f2;
                case "/" -> f1 / f2;
                case "%" -> f1 % f2;
                default -> throw new SemanticException("Invalid operator: " + op);
            };
        } else if (left instanceof Integer && right instanceof Integer) {
            int i1 = (Integer) left;
            int i2 = (Integer) right;
            return switch (op) {
                case "+" -> i1 + i2;
                case "-" -> i1 - i2;
                case "*" -> i1 * i2;
                case "/" -> i1 / i2;
                case "%" -> i1 % i2;
                default -> throw new SemanticException("Invalid operator: " + op);
            };
        } else {
            throw new SemanticException("Something went wrong");
        }
    }
}

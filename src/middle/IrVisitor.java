package middle;

import antlr.SysY2022BaseVisitor;
import antlr.SysY2022Parser;
import exceptions.SemanticException;
import middle.InitVal.ArrayInitVal;
import middle.InitVal.ValueInitVal;
import middle.IrType.ArrayType;
import middle.IrType.FloatType;
import middle.IrType.IntegerType;
import middle.IrType.VoidType;
import middle.llvm.BasicBlock;
import middle.llvm.ConstantValue;
import middle.llvm.Function;
import middle.llvm.GlobalVar;
import middle.llvm.Instruction;
import middle.llvm.IrModule;
import middle.llvm.Value;
import middle.symbol.Symbol;
import middle.symbol.SymbolTable;
import org.antlr.v4.runtime.tree.ParseTree;
import utils.Calculator;
import utils.Pair;

import java.util.ArrayList;

public class IrVisitor extends SysY2022BaseVisitor<Void> {
    // 标记当前函数与当前基本块
    private final IrModule irModule = IrModule.getInstance();
    // 标记当前是否是全局状态
    private boolean isGlobal = false;
    private SymbolTable curSymbolTable = new SymbolTable(null);
    private Function curFunction = null;
    private BasicBlock curBlock = null;

    @Override
    public Void visitCompUnit(SysY2022Parser.CompUnitContext ctx) {
        for (ParseTree node : ctx.children) {
            if (node instanceof SysY2022Parser.DeclContext) {
                isGlobal = true;
                visit(node);
                isGlobal = false;
            } else if (node instanceof SysY2022Parser.FuncDefContext) {
                visit(node);
            }
        }
        return null;
    }

    // 解析常量定义
    @Override
    public Void visitConstDef(SysY2022Parser.ConstDefContext ctx) {
        IrType irType = switch (ctx.type) {
            case "int" -> IntegerType.i32;
            case "float" -> FloatType.f32;
            default -> throw new SemanticException("Illegal type: " + ctx.type);
        };
        ArrayList<Integer> dimensions = new ArrayList<>();
        for (SysY2022Parser.ConstExpContext constExpContext : ctx.constExp()) {
            int dim = (int) Calculator.evalExp(constExpContext.addExp(), curSymbolTable);
            dimensions.add(dim);
        }
        for (int i = dimensions.size() - 1; i >= 0; i--) {
            irType = new ArrayType(dimensions.get(i), irType);
        }
        // constDef一定配有constInitVal
        InitVal initVal;
        if (irType.isI32Type() || irType.isFloatType()) {
            if (!ctx.constInitVal().constInitVal().isEmpty()) {
                throw new SemanticException("Variable cannot be initialized by an array");
            }
            initVal = ValueInitVal.parseValueInitVal(irType, ctx.constInitVal().constExp(), curSymbolTable);
        } else if (irType.isArrayType()) {
            if (ctx.constInitVal().constExp() != null) {
                throw new SemanticException("Array cannot be initialized by a single constant");
            }
            initVal = ArrayInitVal.parseArrayInitVal(irType, ctx.constInitVal(), curSymbolTable);
        } else {
            throw new SemanticException("Illegal type: " + ctx.type);
        }
        // 全局作用域下，生成globalVar对象，否则生成alloca指令
        Value address;
        if (isGlobal) {
            GlobalVar gv = new GlobalVar(
                    irType, ctx.IDENT().getText(), initVal, true
            );
            irModule.addGlobalVar(gv);
            address = gv;
        } else {
            Instruction.Alloca alloca = new Instruction.Alloca(irType, curBlock);
            address = alloca;
            if (irType.isI32Type() || irType.isFloatType()) {
                assert initVal instanceof ValueInitVal;
                new Instruction.Store(alloca, ((ValueInitVal) initVal).getValue(), curBlock);
            } else if (irType.isArrayType()) {
                assert initVal instanceof ArrayInitVal;
                // 将initVal中的所有ValueInitVal展平
                ArrayList<ValueInitVal> flattened = ((ArrayInitVal) initVal).getFlattenedInitVals();
                int[] strides = new int[dimensions.size()];
                int stride = 1;
                for (int i = dimensions.size() - 1; i >= 0; i--) {
                    strides[i] = stride;
                    stride *= dimensions.get(i);
                }
                for (int i = 0; i < flattened.size(); i++) {
                    ValueInitVal val = flattened.get(i);
                    int remaining = i;
                    ArrayList<Integer> indices = new ArrayList<>();
                    for (int j = 0; j < dimensions.size(); j++) {
                        int idx = remaining / strides[j];
                        indices.add(idx);
                        remaining %= strides[j];
                    }
                    Value ptr = address;
                    for (int idx : indices) {
                        ptr = new Instruction.GetElementPtr(ptr,
                                new ConstantValue.ConstantInt(idx), curBlock);
                    }
                    new Instruction.Store(ptr, val.getValue(), curBlock);
                }
            }
        }
        // 向符号表中插入符号
        curSymbolTable.put(new Symbol(
                ctx.IDENT().getText(), irType, true, initVal, address
        ));
        return null;
    }

    @Override
    public Void visitVarDef(SysY2022Parser.VarDefContext ctx) {
        // TODO: 解析普通变量定义
        IrType irType = switch (ctx.type) {
            case "int" -> IntegerType.i32;
            case "float" -> FloatType.f32;
            default -> throw new SemanticException("Illegal type: " + ctx.type);
        };
        ArrayList<Integer> dimensions = new ArrayList<>();
        for (SysY2022Parser.ConstExpContext constExpContext : ctx.constExp()) {
            int dim = (int) Calculator.evalExp(constExpContext.addExp(), curSymbolTable);
            dimensions.add(dim);
        }
        for (int i = dimensions.size() - 1; i >= 0; i--) {
            irType = new ArrayType(dimensions.get(i), irType);
        }
        return null;
    }

    @Override
    public Void visitFuncDef(SysY2022Parser.FuncDefContext ctx) {
        IrType irType = switch (ctx.funcType.getText()) {
            case "int" -> IntegerType.i32;
            case "float" -> FloatType.f32;
            case "void" -> VoidType.VOID;
            default -> throw new SemanticException("Illegal type: " + ctx.funcType.getText());
        };
        String ident = ctx.IDENT().getText();
        if (curSymbolTable.get(ident) != null) {
            throw new SemanticException("Duplicate ident: " + ident);
        }
        // 创建局部符号表
        curSymbolTable = new SymbolTable(curSymbolTable);
        // 如果存在形参，获取函数形参的种类
        ArrayList<Pair<String, IrType>> arguments = new ArrayList<>();
        if (ctx.funcFParams() != null) {
            SysY2022Parser.FuncFParamsContext funcFParams = ctx.funcFParams();
            for (SysY2022Parser.FuncFParamContext funcFParamContext : funcFParams.funcFParam()) {
                arguments.add(parseFuncFParam(funcFParamContext));
            }
        }
        // 创建llvm Function和第一个block
        Function function = new Function(irType, ident);
        curFunction = function;
        curBlock = new BasicBlock(function);
        // 添加参数并进行检查
        for (int i = 0; i < arguments.size(); i++) {
            Pair<String, IrType> argument = arguments.get(i);
            if (curSymbolTable.get(argument.first) != null) {
                throw new SemanticException("Duplicate ident: " + argument.first);
            }
            Function.FuncArgument funcArgument = new Function.FuncArgument(argument.second, i);
            function.addArgument(funcArgument);
        }
        for (int i = 0; i < arguments.size(); i++) {
            Pair<String, IrType> argument = arguments.get(i);
            Instruction.Alloca address = new Instruction.Alloca(argument.second, curBlock);
            new Instruction.Store(address, function.getArgument(i), curBlock);
            curSymbolTable.put(new Symbol(argument.first, argument.second, false, null, address));
        }
        irModule.addFunction(function);
        visit(ctx.block());
        return null;
    }

    private Pair<String, IrType> parseFuncFParam(SysY2022Parser.FuncFParamContext ctx) {
        IrType atomicType = switch (ctx.bType.getText()) {
            case "int" -> IntegerType.i32;
            case "float" -> FloatType.f32;
            default -> throw new SemanticException("Illegal type: " + ctx.bType.getText());
        };
        String ident = ctx.IDENT().getText();
        // 不带有"[]"说明是普通形参
        if (!ctx.getText().contains("[]")) {
            return new Pair<>(ident, atomicType);
        }
        // 对于数组形参，需要解析exp的值
        // 此处exp的值应在编译期内即可得出
        IrType type = atomicType;
        ArrayList<Integer> dims = new ArrayList<>();
        for (SysY2022Parser.ExpContext expContext : ctx.exp()) {
            int dim = (int) Calculator.evalExp(expContext.addExp(), curSymbolTable);
            if (dim <= 0) {
                throw new SemanticException("Illegal dimension: " + dim);
            }
            dims.add(dim);
        }
        // 反向递归构建参数类型
        for (int i = dims.size() - 1; i >= 0; i--) {
            type = new ArrayType(dims.get(i), type);
        }
        return new Pair<>(ident, new IrType.PointerType(type));
    }

    @Override
    public Void visitAssignStmt(SysY2022Parser.AssignStmtContext ctx) {
        Symbol symbol = curSymbolTable.search(ctx.lVal().IDENT().getText());
        // 检查变量名是否存在于符号表中
        if (symbol == null) {
            throw new SemanticException("Ident undefined: " + ctx.lVal().IDENT().getText());
        }
        if (symbol.isConstant()) {
            throw new SemanticException("Constant " + ctx.lVal().IDENT().getText() + " cannot be assigned");
        }
        // 获取等号左边的地址
        Value address = parseLVal(ctx.lVal(), true);
        return null;
    }

    private Value parseLVal(SysY2022Parser.LValContext ctx, boolean getAddress) {
        // 对LVal的解析存在两种情况：取地址与取值
        // 通过传入的getAddress参数进行判断，需要获取地址还是获取值
        Symbol symbol = curSymbolTable.search(ctx.IDENT().getText());
        if (symbol == null) {
            throw new SemanticException("Ident undefined: " + ctx.IDENT().getText());
        }
        if (symbol.isConstant()) {
            throw new SemanticException("Constant " + ctx.IDENT().getText() + " cannot be assigned");
        }
        // 对于数组变量或一维变量，首先获取基地址
        Value baseAddress = symbol.getAddress();
        if (!(baseAddress.getType() instanceof IrType.PointerType pointerType)) {
            throw new SemanticException("Illegal address: " + baseAddress);
        }
        IrType type = pointerType.getTargetType();
        // 如果是一维变量
        if (type.isI32Type() || type.isFloatType()) {
            if (getAddress) {
                return baseAddress;
            } else {
                return new Instruction.Load(baseAddress, curBlock);
            }
        }
        // 如果是数组变量，需要对下标进行解析
        ArrayList<Value> indexes = new ArrayList<>();
        for (SysY2022Parser.ExpContext expContext : ctx.exp()) {
            Value index = parseExp(expContext);
        }
        return null;
    }

    private Value parseExp(SysY2022Parser.ExpContext ctx) {
        return parseAddExp(ctx.addExp());
    }

    private Value parseAddExp(SysY2022Parser.AddExpContext ctx) {
        return null;
    }

    private Value parseMulExp(SysY2022Parser.MulExpContext ctx) {
        return null;
    }

    private Value parseUnaryExp(SysY2022Parser.UnaryExpContext ctx) {
        return null;
    }
}

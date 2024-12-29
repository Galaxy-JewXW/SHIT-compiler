// Generated from D:/BUAA_Undergraduate/projects/ggb/SysY2022.g4 by ANTLR 4.13.2
package antlr;
import org.antlr.v4.runtime.tree.ParseTreeVisitor;

/**
 * This interface defines a complete generic visitor for a parse tree produced
 * by {@link SysY2022Parser}.
 *
 * @param <T> The return type of the visit operation. Use {@link Void} for
 * operations with no return type.
 */
public interface SysY2022Visitor<T> extends ParseTreeVisitor<T> {
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#compUnit}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitCompUnit(SysY2022Parser.CompUnitContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#decl}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitDecl(SysY2022Parser.DeclContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#constDecl}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitConstDecl(SysY2022Parser.ConstDeclContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#varDecl}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitVarDecl(SysY2022Parser.VarDeclContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#constDef}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitConstDef(SysY2022Parser.ConstDefContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#constInitVal}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitConstInitVal(SysY2022Parser.ConstInitValContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#varDef}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitVarDef(SysY2022Parser.VarDefContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#initVal}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitInitVal(SysY2022Parser.InitValContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#funcDef}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitFuncDef(SysY2022Parser.FuncDefContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#funcFParams}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitFuncFParams(SysY2022Parser.FuncFParamsContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#funcFParam}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitFuncFParam(SysY2022Parser.FuncFParamContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#block}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitBlock(SysY2022Parser.BlockContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#blockItem}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitBlockItem(SysY2022Parser.BlockItemContext ctx);
	/**
	 * Visit a parse tree produced by the {@code assignStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitAssignStmt(SysY2022Parser.AssignStmtContext ctx);
	/**
	 * Visit a parse tree produced by the {@code putfStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitPutfStmt(SysY2022Parser.PutfStmtContext ctx);
	/**
	 * Visit a parse tree produced by the {@code expStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitExpStmt(SysY2022Parser.ExpStmtContext ctx);
	/**
	 * Visit a parse tree produced by the {@code blockStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitBlockStmt(SysY2022Parser.BlockStmtContext ctx);
	/**
	 * Visit a parse tree produced by the {@code ifStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitIfStmt(SysY2022Parser.IfStmtContext ctx);
	/**
	 * Visit a parse tree produced by the {@code whileStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitWhileStmt(SysY2022Parser.WhileStmtContext ctx);
	/**
	 * Visit a parse tree produced by the {@code breakStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitBreakStmt(SysY2022Parser.BreakStmtContext ctx);
	/**
	 * Visit a parse tree produced by the {@code continueStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitContinueStmt(SysY2022Parser.ContinueStmtContext ctx);
	/**
	 * Visit a parse tree produced by the {@code returnStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitReturnStmt(SysY2022Parser.ReturnStmtContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#exp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitExp(SysY2022Parser.ExpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#cond}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitCond(SysY2022Parser.CondContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#lVal}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitLVal(SysY2022Parser.LValContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#primaryExp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitPrimaryExp(SysY2022Parser.PrimaryExpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#number}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitNumber(SysY2022Parser.NumberContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#unaryExp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitUnaryExp(SysY2022Parser.UnaryExpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#unaryOp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitUnaryOp(SysY2022Parser.UnaryOpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#funcRParams}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitFuncRParams(SysY2022Parser.FuncRParamsContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#mulExp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitMulExp(SysY2022Parser.MulExpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#addExp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitAddExp(SysY2022Parser.AddExpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#relExp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitRelExp(SysY2022Parser.RelExpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#eqExp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitEqExp(SysY2022Parser.EqExpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#lAndExp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitLAndExp(SysY2022Parser.LAndExpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#lOrExp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitLOrExp(SysY2022Parser.LOrExpContext ctx);
	/**
	 * Visit a parse tree produced by {@link SysY2022Parser#constExp}.
	 * @param ctx the parse tree
	 * @return the visitor result
	 */
	T visitConstExp(SysY2022Parser.ConstExpContext ctx);
}
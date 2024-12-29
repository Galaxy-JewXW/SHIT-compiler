// Generated from D:/BUAA_Undergraduate/projects/ggb/SysY2022.g4 by ANTLR 4.13.2
package antlr;
import org.antlr.v4.runtime.tree.ParseTreeListener;

/**
 * This interface defines a complete listener for a parse tree produced by
 * {@link SysY2022Parser}.
 */
public interface SysY2022Listener extends ParseTreeListener {
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#compUnit}.
	 * @param ctx the parse tree
	 */
	void enterCompUnit(SysY2022Parser.CompUnitContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#compUnit}.
	 * @param ctx the parse tree
	 */
	void exitCompUnit(SysY2022Parser.CompUnitContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#decl}.
	 * @param ctx the parse tree
	 */
	void enterDecl(SysY2022Parser.DeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#decl}.
	 * @param ctx the parse tree
	 */
	void exitDecl(SysY2022Parser.DeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#constDecl}.
	 * @param ctx the parse tree
	 */
	void enterConstDecl(SysY2022Parser.ConstDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#constDecl}.
	 * @param ctx the parse tree
	 */
	void exitConstDecl(SysY2022Parser.ConstDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#varDecl}.
	 * @param ctx the parse tree
	 */
	void enterVarDecl(SysY2022Parser.VarDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#varDecl}.
	 * @param ctx the parse tree
	 */
	void exitVarDecl(SysY2022Parser.VarDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#constDef}.
	 * @param ctx the parse tree
	 */
	void enterConstDef(SysY2022Parser.ConstDefContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#constDef}.
	 * @param ctx the parse tree
	 */
	void exitConstDef(SysY2022Parser.ConstDefContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#constInitVal}.
	 * @param ctx the parse tree
	 */
	void enterConstInitVal(SysY2022Parser.ConstInitValContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#constInitVal}.
	 * @param ctx the parse tree
	 */
	void exitConstInitVal(SysY2022Parser.ConstInitValContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#varDef}.
	 * @param ctx the parse tree
	 */
	void enterVarDef(SysY2022Parser.VarDefContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#varDef}.
	 * @param ctx the parse tree
	 */
	void exitVarDef(SysY2022Parser.VarDefContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#initVal}.
	 * @param ctx the parse tree
	 */
	void enterInitVal(SysY2022Parser.InitValContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#initVal}.
	 * @param ctx the parse tree
	 */
	void exitInitVal(SysY2022Parser.InitValContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#funcDef}.
	 * @param ctx the parse tree
	 */
	void enterFuncDef(SysY2022Parser.FuncDefContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#funcDef}.
	 * @param ctx the parse tree
	 */
	void exitFuncDef(SysY2022Parser.FuncDefContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#funcFParams}.
	 * @param ctx the parse tree
	 */
	void enterFuncFParams(SysY2022Parser.FuncFParamsContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#funcFParams}.
	 * @param ctx the parse tree
	 */
	void exitFuncFParams(SysY2022Parser.FuncFParamsContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#funcFParam}.
	 * @param ctx the parse tree
	 */
	void enterFuncFParam(SysY2022Parser.FuncFParamContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#funcFParam}.
	 * @param ctx the parse tree
	 */
	void exitFuncFParam(SysY2022Parser.FuncFParamContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#block}.
	 * @param ctx the parse tree
	 */
	void enterBlock(SysY2022Parser.BlockContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#block}.
	 * @param ctx the parse tree
	 */
	void exitBlock(SysY2022Parser.BlockContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#blockItem}.
	 * @param ctx the parse tree
	 */
	void enterBlockItem(SysY2022Parser.BlockItemContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#blockItem}.
	 * @param ctx the parse tree
	 */
	void exitBlockItem(SysY2022Parser.BlockItemContext ctx);
	/**
	 * Enter a parse tree produced by the {@code assignStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void enterAssignStmt(SysY2022Parser.AssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code assignStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void exitAssignStmt(SysY2022Parser.AssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code putfStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void enterPutfStmt(SysY2022Parser.PutfStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code putfStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void exitPutfStmt(SysY2022Parser.PutfStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code expStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void enterExpStmt(SysY2022Parser.ExpStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code expStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void exitExpStmt(SysY2022Parser.ExpStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code blockStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void enterBlockStmt(SysY2022Parser.BlockStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code blockStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void exitBlockStmt(SysY2022Parser.BlockStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code ifStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void enterIfStmt(SysY2022Parser.IfStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code ifStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void exitIfStmt(SysY2022Parser.IfStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code whileStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void enterWhileStmt(SysY2022Parser.WhileStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code whileStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void exitWhileStmt(SysY2022Parser.WhileStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code breakStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void enterBreakStmt(SysY2022Parser.BreakStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code breakStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void exitBreakStmt(SysY2022Parser.BreakStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code continueStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void enterContinueStmt(SysY2022Parser.ContinueStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code continueStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void exitContinueStmt(SysY2022Parser.ContinueStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code returnStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void enterReturnStmt(SysY2022Parser.ReturnStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code returnStmt}
	 * labeled alternative in {@link SysY2022Parser#stmt}.
	 * @param ctx the parse tree
	 */
	void exitReturnStmt(SysY2022Parser.ReturnStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#exp}.
	 * @param ctx the parse tree
	 */
	void enterExp(SysY2022Parser.ExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#exp}.
	 * @param ctx the parse tree
	 */
	void exitExp(SysY2022Parser.ExpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#cond}.
	 * @param ctx the parse tree
	 */
	void enterCond(SysY2022Parser.CondContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#cond}.
	 * @param ctx the parse tree
	 */
	void exitCond(SysY2022Parser.CondContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#lVal}.
	 * @param ctx the parse tree
	 */
	void enterLVal(SysY2022Parser.LValContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#lVal}.
	 * @param ctx the parse tree
	 */
	void exitLVal(SysY2022Parser.LValContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#primaryExp}.
	 * @param ctx the parse tree
	 */
	void enterPrimaryExp(SysY2022Parser.PrimaryExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#primaryExp}.
	 * @param ctx the parse tree
	 */
	void exitPrimaryExp(SysY2022Parser.PrimaryExpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#number}.
	 * @param ctx the parse tree
	 */
	void enterNumber(SysY2022Parser.NumberContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#number}.
	 * @param ctx the parse tree
	 */
	void exitNumber(SysY2022Parser.NumberContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#unaryExp}.
	 * @param ctx the parse tree
	 */
	void enterUnaryExp(SysY2022Parser.UnaryExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#unaryExp}.
	 * @param ctx the parse tree
	 */
	void exitUnaryExp(SysY2022Parser.UnaryExpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#unaryOp}.
	 * @param ctx the parse tree
	 */
	void enterUnaryOp(SysY2022Parser.UnaryOpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#unaryOp}.
	 * @param ctx the parse tree
	 */
	void exitUnaryOp(SysY2022Parser.UnaryOpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#funcRParams}.
	 * @param ctx the parse tree
	 */
	void enterFuncRParams(SysY2022Parser.FuncRParamsContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#funcRParams}.
	 * @param ctx the parse tree
	 */
	void exitFuncRParams(SysY2022Parser.FuncRParamsContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#mulExp}.
	 * @param ctx the parse tree
	 */
	void enterMulExp(SysY2022Parser.MulExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#mulExp}.
	 * @param ctx the parse tree
	 */
	void exitMulExp(SysY2022Parser.MulExpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#addExp}.
	 * @param ctx the parse tree
	 */
	void enterAddExp(SysY2022Parser.AddExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#addExp}.
	 * @param ctx the parse tree
	 */
	void exitAddExp(SysY2022Parser.AddExpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#relExp}.
	 * @param ctx the parse tree
	 */
	void enterRelExp(SysY2022Parser.RelExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#relExp}.
	 * @param ctx the parse tree
	 */
	void exitRelExp(SysY2022Parser.RelExpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#eqExp}.
	 * @param ctx the parse tree
	 */
	void enterEqExp(SysY2022Parser.EqExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#eqExp}.
	 * @param ctx the parse tree
	 */
	void exitEqExp(SysY2022Parser.EqExpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#lAndExp}.
	 * @param ctx the parse tree
	 */
	void enterLAndExp(SysY2022Parser.LAndExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#lAndExp}.
	 * @param ctx the parse tree
	 */
	void exitLAndExp(SysY2022Parser.LAndExpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#lOrExp}.
	 * @param ctx the parse tree
	 */
	void enterLOrExp(SysY2022Parser.LOrExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#lOrExp}.
	 * @param ctx the parse tree
	 */
	void exitLOrExp(SysY2022Parser.LOrExpContext ctx);
	/**
	 * Enter a parse tree produced by {@link SysY2022Parser#constExp}.
	 * @param ctx the parse tree
	 */
	void enterConstExp(SysY2022Parser.ConstExpContext ctx);
	/**
	 * Exit a parse tree produced by {@link SysY2022Parser#constExp}.
	 * @param ctx the parse tree
	 */
	void exitConstExp(SysY2022Parser.ConstExpContext ctx);
}
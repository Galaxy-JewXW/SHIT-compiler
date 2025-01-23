#ifndef BUILDER_H
#define BUILDER_H
#include <memory>

#include "Structure.h"
#include "Symbol.h"
#include "Utils/AST.h"

namespace Mir {
class Builder {
    bool is_global{false};
    std::shared_ptr<Module> module = std::make_shared<Module>();
    std::shared_ptr<Symbol::Table> table = std::make_shared<Symbol::Table>();
    std::shared_ptr<Function> cur_function;
    std::shared_ptr<Block> cur_block;

public:
    explicit Builder() { table->push_scope(); }

    std::shared_ptr<Module> visit(const std::shared_ptr<AST::CompUnit> &ast);

    void visit_decl(const std::shared_ptr<AST::Decl> &decl);

    void visit_constDecl(const std::shared_ptr<AST::ConstDecl> &constDecl);

    void visit_constDef(const std::shared_ptr<AST::ConstDef> &constDef);

    void visit_constInitVal(const std::shared_ptr<AST::ConstInitVal> &constInitVal);

    void visit_varDecl(const std::shared_ptr<AST::VarDecl> &varDecl);

    void visit_varDef(const std::shared_ptr<AST::VarDef> &varDef);

    void visit_initVal(const std::shared_ptr<AST::InitVal> &initVal);

    void visit_funcDef(const std::shared_ptr<AST::FuncDef> &funcDef);

    void visit_funcFParam(const std::shared_ptr<AST::FuncFParam> &funcFParam);

    void visit_block(const std::shared_ptr<AST::Block> &block);

    void visit_stmt(const std::shared_ptr<AST::Stmt> &stmt);

    void visit_assignStmt(const std::shared_ptr<AST::AssignStmt> &assignStmt);

    void visit_expStmt(const std::shared_ptr<AST::ExpStmt> &expStmt);

    void visit_blockStmt(const std::shared_ptr<AST::BlockStmt> &blockStmt);

    void visit_ifStmt(const std::shared_ptr<AST::IfStmt> &ifStmt);

    void visit_whileStmt(const std::shared_ptr<AST::WhileStmt> &whileStmt);

    void visit_breakStmt(const std::shared_ptr<AST::BreakStmt> &breakStmt);

    void visit_continueStmt(const std::shared_ptr<AST::ContinueStmt> &continueStmt);

    void visit_returnStmt(const std::shared_ptr<AST::ReturnStmt> &returnStmt);

    void visit_exp(const std::shared_ptr<AST::Exp> &exp);

    void visit_cond(const std::shared_ptr<AST::Cond> &cond);

    void visit_lVal(const std::shared_ptr<AST::LVal> &lVal);

    void visit_primaryExp(const std::shared_ptr<AST::PrimaryExp> &primaryExp);

    void visit_unaryExp(const std::shared_ptr<AST::UnaryExp> &unaryExp);

    void visit_mulExp(const std::shared_ptr<AST::MulExp> &mulExp);

    void visit_addExp(const std::shared_ptr<AST::AddExp> &addExp);

    void visit_relExp(const std::shared_ptr<AST::RelExp> &relExp);

    void visit_eqExp(const std::shared_ptr<AST::EqExp> &eqExp);

    void visit_lAndExp(const std::shared_ptr<AST::LAndExp> &lAndExp);

    void visit_lOrExp(const std::shared_ptr<AST::LOrExp> &lOrExp);

    void visit_constExp(const std::shared_ptr<AST::ConstExp> &constExp);
};
}

#endif

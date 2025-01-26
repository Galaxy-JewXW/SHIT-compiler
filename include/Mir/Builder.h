#ifndef BUILDER_H
#define BUILDER_H
#include <memory>

#include "Const.h"
#include "Structure.h"
#include "Symbol.h"
#include "Utils/AST.h"

namespace Mir {
class Builder {
    static size_t variable_count, block_count;
    bool is_global{false};
    std::shared_ptr<Module> module = std::make_shared<Module>();
    std::shared_ptr<Symbol::Table> table = std::make_shared<Symbol::Table>();
    std::shared_ptr<Function> cur_function;
    std::shared_ptr<Block> cur_block;

public:
    explicit Builder() { table->push_scope(); }

    static std::string gen_variable_name() { return "%" + std::to_string(variable_count++); }

    static std::string gen_block_name() { return "block_" + std::to_string(block_count++); }

    static void reset_count() {
        variable_count = 0;
        block_count = 0;
    }

    [[nodiscard]] std::shared_ptr<Module> &visit(const std::shared_ptr<AST::CompUnit> &ast);

    void visit_decl(const std::shared_ptr<AST::Decl> &decl) const;

    void visit_constDecl(const std::shared_ptr<AST::ConstDecl> &constDecl) const;

    void visit_constDef(Token::Type type, const std::shared_ptr<AST::ConstDef> &constDef) const;

    void visit_varDecl(const std::shared_ptr<AST::VarDecl> &varDecl) const;

    void visit_varDef(Token::Type type, const std::shared_ptr<AST::VarDef> &varDef) const;

    void visit_funcDef(const std::shared_ptr<AST::FuncDef> &funcDef);

    [[nodiscard]] std::pair<std::string, std::shared_ptr<Type::Type>>
    visit_funcFParam(const std::shared_ptr<AST::FuncFParam> &funcFParam) const;

    void visit_block(const std::shared_ptr<AST::Block> &block) const;

    void visit_stmt(const std::shared_ptr<AST::Stmt> &stmt);

    void visit_assignStmt(const std::shared_ptr<AST::AssignStmt> &assignStmt);

    void visit_expStmt(const std::shared_ptr<AST::ExpStmt> &expStmt);

    void visit_blockStmt(const std::shared_ptr<AST::BlockStmt> &blockStmt);

    void visit_ifStmt(const std::shared_ptr<AST::IfStmt> &ifStmt);

    void visit_whileStmt(const std::shared_ptr<AST::WhileStmt> &whileStmt);

    void visit_breakStmt(const std::shared_ptr<AST::BreakStmt> &breakStmt);

    void visit_continueStmt(const std::shared_ptr<AST::ContinueStmt> &continueStmt);

    void visit_returnStmt(const std::shared_ptr<AST::ReturnStmt> &returnStmt);

    [[nodiscard]] std::shared_ptr<Value> visit_exp(const std::shared_ptr<AST::Exp> &exp) const;

    std::shared_ptr<Value> visit_cond(const std::shared_ptr<AST::Cond> &cond);

    [[nodiscard]] std::shared_ptr<Value> visit_lVal(const std::shared_ptr<AST::LVal> &lVal,
                                                    bool get_address = false) const;

    [[nodiscard]] static std::shared_ptr<Value> visit_number(const std::shared_ptr<AST::Number> &number);

    [[nodiscard]] std::shared_ptr<Value> visit_primaryExp(const std::shared_ptr<AST::PrimaryExp> &primaryExp) const;

    [[nodiscard]] std::shared_ptr<Value> visit_unaryExp(const std::shared_ptr<AST::UnaryExp> &unaryExp) const;

    [[nodiscard]] std::shared_ptr<Value> visit_mulExp(const std::shared_ptr<AST::MulExp> &mulExp) const;

    [[nodiscard]] std::shared_ptr<Value> visit_addExp(const std::shared_ptr<AST::AddExp> &addExp) const;

    std::shared_ptr<Value> visit_relExp(const std::shared_ptr<AST::RelExp> &relExp);

    std::shared_ptr<Value> visit_eqExp(const std::shared_ptr<AST::EqExp> &eqExp);

    std::shared_ptr<Value> visit_lAndExp(const std::shared_ptr<AST::LAndExp> &lAndExp);

    std::shared_ptr<Value> visit_lOrExp(const std::shared_ptr<AST::LOrExp> &lOrExp);

    std::shared_ptr<Value> visit_constExp(const std::shared_ptr<AST::ConstExp> &constExp);
};

// 用于自动类型转换
std::shared_ptr<Value> type_cast(const std::shared_ptr<Value> &v, const std::shared_ptr<Type::Type> &target_type,
                                 const std::shared_ptr<Block> &block);
}

// 用于在编译期内计算常数
using EvalResult = std::variant<int, float>;

EvalResult eval_exp(const std::shared_ptr<AST::AddExp> &exp, const std::shared_ptr<Mir::Symbol::Table> &table);

#endif

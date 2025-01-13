#ifndef PARSER_H
#define PARSER_H

#include "Utils/AST.h"
#include "Utils/Token.h"

class Parser {
    // lexer得到的
    const std::vector<Token::Token> tokens;
    // 当前指针位置
    size_t pos;
    // 当前字符
    [[nodiscard]] Token::Token peek() const {
        return tokens.at(pos);
    }

    // 向前(后)读n个字符
    [[nodiscard]] Token::Token next(const int offset = 1) const {
        return tokens.at(pos + offset);
    }

    // 如果该位置不是符合要求的Token，抛出异常；否则pos向前移动
    template<typename... Types>
    bool panic_on(Types... expected_types);

    // 与match类似，如果不合要求将忽略
    template<typename... Types>
    bool match(Types... expected_types);

    std::shared_ptr<AST::CompUnit> parseCompUnit();

    std::shared_ptr<AST::Decl> parseDecl();

    std::shared_ptr<AST::ConstDecl> parseConstDecl();

    std::shared_ptr<AST::ConstDef> parseConstDef();

    std::shared_ptr<AST::ConstInitVal> parseConstInitVal();

    std::shared_ptr<AST::VarDecl> parseVarDecl();

    std::shared_ptr<AST::VarDef> parseVarDef();

    std::shared_ptr<AST::InitVal> parseInitVal();

    std::shared_ptr<AST::FuncDef> parseFuncDef();

    std::shared_ptr<AST::FuncFParam> parseFuncFParam();

    std::shared_ptr<AST::Block> parseBlock();

    std::shared_ptr<AST::Stmt> parseStmt();

    std::shared_ptr<AST::ReturnStmt> parseReturnStmt();

    std::shared_ptr<AST::IfStmt> parseIfStmt();

    std::shared_ptr<AST::WhileStmt> parseWhileStmt();

    std::shared_ptr<AST::Exp> parseExp();

    std::shared_ptr<AST::Cond> parseCond();

    std::shared_ptr<AST::LVal> parseLVal();

    std::shared_ptr<AST::PrimaryExp> parsePrimaryExp();

    [[nodiscard]] std::shared_ptr<AST::Number> parseNumber() const;

    std::shared_ptr<AST::UnaryExp> parseUnaryExp();

    std::shared_ptr<AST::MulExp> parseMulExp();

    std::shared_ptr<AST::AddExp> parseAddExp();

    std::shared_ptr<AST::RelExp> parseRelExp();

    std::shared_ptr<AST::EqExp> parseEqExp();

    std::shared_ptr<AST::LAndExp> parseLAndExp();

    std::shared_ptr<AST::LOrExp> parseLOrExp();

    std::shared_ptr<AST::ConstExp> parseConstExp();

public:
    explicit Parser(const std::vector<Token::Token> &tokens) : tokens{tokens}, pos{0} {}

    std::shared_ptr<AST::CompUnit> parse() {
        return parseCompUnit();
    }

    [[nodiscard]] bool eof() const {
        return tokens[pos].type == Token::Type::END_OF_FILE;
    }
};

#endif //PARSER_H

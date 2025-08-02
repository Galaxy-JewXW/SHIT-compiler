#include "Frontend/Parser.h"
#include "Utils/AST.h"
#include "Utils/Log.h"

#include <unordered_set>

template<typename... Types>
bool Parser::panic_on(Types... expected_types) {
    std::unordered_set<Token::Type> types = {expected_types...};
    if (const Token::Type current_type = peek().type; types.find(current_type) == types.end()) {
        std::ostringstream oss;
        oss << "Expected one of { ";
        for (const auto &type: types) {
            oss << type_to_string(type) << " ";
        }
        oss << "}, got Token " << type_to_string(current_type) << " at line " << peek().line;
        log_fatal(oss.str().c_str());
    }
    pos++;
    return true;
}

template<typename... Types>
bool Parser::match(Types... expected_types) {
    std::unordered_set<Token::Type> types = {expected_types...};
    if (const Token::Type current_type = peek().type; types.find(current_type) == types.end())
        return false;
    pos++;
    return true;
}

std::shared_ptr<AST::CompUnit> Parser::parseCompUnit() {
    std::vector<std::variant<std::shared_ptr<AST::Decl>, std::shared_ptr<AST::FuncDef>>> compunits;
    while (!eof()) {
        if (next(2).type == Token::Type::LPAREN) {
            compunits.emplace_back(parseFuncDef());
        } else {
            compunits.emplace_back(parseDecl());
        }
    }
    panic_on(Token::Type::END_OF_FILE);
    return std::make_shared<AST::CompUnit>(compunits);
}

std::shared_ptr<AST::Decl> Parser::parseDecl() {
    if (peek().type == Token::Type::CONST) {
        return parseConstDecl();
    }
    return parseVarDecl();
}

std::shared_ptr<AST::ConstDecl> Parser::parseConstDecl() {
    panic_on(Token::Type::CONST);
    panic_on(Token::Type::INT, Token::Type::FLOAT);
    Token::Type bType = next(-1).type;
    std::vector<std::shared_ptr<AST::ConstDef>> constDefs;
    do {
        constDefs.emplace_back(parseConstDef());
    } while (match(Token::Type::COMMA));
    panic_on(Token::Type::SEMICOLON);
    return std::make_shared<AST::ConstDecl>(bType, constDefs);
}

std::shared_ptr<AST::ConstDef> Parser::parseConstDef() {
    panic_on(Token::Type::IDENTIFIER);
    const std::string ident = next(-1).content;
    std::vector<std::shared_ptr<AST::ConstExp>> constExps;
    if (match(Token::Type::LBRACKET)) {
        do {
            constExps.emplace_back(parseConstExp());
            panic_on(Token::Type::RBRACKET);
        } while (match(Token::Type::LBRACKET));
    }
    panic_on(Token::Type::ASSIGN);
    std::shared_ptr<AST::ConstInitVal> constInitVal = parseConstInitVal();
    return std::make_shared<AST::ConstDef>(ident, constExps, constInitVal);
}

std::shared_ptr<AST::ConstInitVal> Parser::parseConstInitVal() {
    if (match(Token::Type::LBRACE)) {
        std::vector<std::shared_ptr<AST::ConstInitVal>> constInitVals;
        if (match(Token::Type::RBRACE)) {
            return std::make_shared<AST::ConstInitVal>(constInitVals);
        }
        do {
            constInitVals.emplace_back(parseConstInitVal());
        } while (match(Token::Type::COMMA));
        panic_on(Token::Type::RBRACE);
        return std::make_shared<AST::ConstInitVal>(constInitVals);
    }
    std::shared_ptr<AST::ConstExp> constExp = parseConstExp();
    return std::make_shared<AST::ConstInitVal>(constExp);
}

std::shared_ptr<AST::VarDecl> Parser::parseVarDecl() {
    panic_on(Token::Type::INT, Token::Type::FLOAT);
    Token::Type bType = next(-1).type;
    std::vector<std::shared_ptr<AST::VarDef>> varDefs;
    do {
        varDefs.emplace_back(parseVarDef());
    } while (match(Token::Type::COMMA));
    panic_on(Token::Type::SEMICOLON);
    return std::make_shared<AST::VarDecl>(bType, varDefs);
}

std::shared_ptr<AST::VarDef> Parser::parseVarDef() {
    panic_on(Token::Type::IDENTIFIER);
    const std::string ident = next(-1).content;
    std::vector<std::shared_ptr<AST::ConstExp>> constExps;
    if (match(Token::Type::LBRACKET)) {
        do {
            constExps.emplace_back(parseConstExp());
            panic_on(Token::Type::RBRACKET);
        } while (match(Token::Type::LBRACKET));
    }
    std::shared_ptr<AST::InitVal> initVal = nullptr;
    if (match(Token::Type::ASSIGN)) {
        initVal = parseInitVal();
    }
    return std::make_shared<AST::VarDef>(ident, constExps, initVal);
}

std::shared_ptr<AST::InitVal> Parser::parseInitVal() {
    if (match(Token::Type::LBRACE)) {
        std::vector<std::shared_ptr<AST::InitVal>> initVals;
        if (match(Token::Type::RBRACE)) {
            return std::make_shared<AST::InitVal>(initVals);
        }
        do {
            initVals.emplace_back(parseInitVal());
        } while (match(Token::Type::COMMA));
        panic_on(Token::Type::RBRACE);
        return std::make_shared<AST::InitVal>(initVals);
    }
    std::shared_ptr<AST::Exp> exp = parseExp();
    return std::make_shared<AST::InitVal>(exp);
}

std::shared_ptr<AST::FuncDef> Parser::parseFuncDef() {
    panic_on(Token::Type::INT, Token::Type::FLOAT, Token::Type::VOID);
    const Token::Type func_type = next(-1).type;
    panic_on(Token::Type::IDENTIFIER);
    const std::string ident = next(-1).content;
    panic_on(Token::Type::LPAREN);
    std::vector<std::shared_ptr<AST::FuncFParam>> funcParams;
    if (peek().type != Token::Type::RPAREN) {
        do {
            funcParams.emplace_back(parseFuncFParam());
        } while (match(Token::Type::COMMA));
    }
    panic_on(Token::Type::RPAREN);
    const auto block = parseBlock();
    return std::make_shared<AST::FuncDef>(func_type, ident, funcParams, block);
}

std::shared_ptr<AST::FuncFParam> Parser::parseFuncFParam() {
    panic_on(Token::Type::INT, Token::Type::FLOAT);
    const Token::Type bType = next(-1).type;
    panic_on(Token::Type::IDENTIFIER);
    const std::string ident = next(-1).content;
    std::vector<std::shared_ptr<AST::Exp>> exps;
    if (peek().type == Token::Type::LBRACKET && next().type == Token::Type::RBRACKET) {
        // 第一维默认为空，向exps插入一个nullptr
        panic_on(Token::Type::LBRACKET);
        panic_on(Token::Type::RBRACKET);
        exps.emplace_back(nullptr);
    }
    while (!(peek().type == Token::Type::RPAREN || peek().type == Token::Type::COMMA)) {
        panic_on(Token::Type::LBRACKET);
        exps.emplace_back(parseExp());
        panic_on(Token::Type::RBRACKET);
    }
    return std::make_shared<AST::FuncFParam>(bType, ident, exps);
}

std::shared_ptr<AST::Block> Parser::parseBlock() {
    panic_on(Token::Type::LBRACE);
    std::vector<std::variant<std::shared_ptr<AST::Decl>, std::shared_ptr<AST::Stmt>>> items;
    while (!match(Token::Type::RBRACE)) {
        if (peek().type == Token::Type::CONST || peek().type == Token::Type::INT || peek().type == Token::Type::FLOAT) {
            items.emplace_back(parseDecl());
        } else {
            items.emplace_back(parseStmt());
        }
    }
    return std::make_shared<AST::Block>(items);
}

std::shared_ptr<AST::Stmt> Parser::parseStmt() {
    if (match(Token::Type::BREAK)) {
        panic_on(Token::Type::SEMICOLON);
        return std::make_shared<AST::BreakStmt>();
    }
    if (match(Token::Type::CONTINUE)) {
        panic_on(Token::Type::SEMICOLON);
        return std::make_shared<AST::ContinueStmt>();
    }
    if (match(Token::Type::RETURN)) {
        return parseReturnStmt();
    }
    if (match(Token::Type::IF)) {
        return parseIfStmt();
    }
    if (match(Token::Type::WHILE)) {
        return parseWhileStmt();
    }
    if (peek().type == Token::Type::LBRACE) {
        const auto block = parseBlock();
        return std::make_shared<AST::BlockStmt>(block);
    }
    if (peek().type == Token::Type::IDENTIFIER) {
        const auto temp = pos;
        const auto lVal = parseLVal();
        if (match(Token::Type::ASSIGN)) {
            const auto exp = parseExp();
            panic_on(Token::Type::SEMICOLON);
            return std::make_shared<AST::AssignStmt>(lVal, exp);
        }
        pos = temp;
    }
    std::shared_ptr<AST::Exp> exp = nullptr;
    if (!match(Token::Type::SEMICOLON)) {
        exp = parseExp();
        panic_on(Token::Type::SEMICOLON);
    }
    return std::make_shared<AST::ExpStmt>(exp);
}

std::shared_ptr<AST::ReturnStmt> Parser::parseReturnStmt() {
    std::shared_ptr<AST::Exp> exp = nullptr;
    if (!match(Token::Type::SEMICOLON)) {
        exp = parseExp();
        panic_on(Token::Type::SEMICOLON);
    }
    return std::make_shared<AST::ReturnStmt>(exp);
}

std::shared_ptr<AST::IfStmt> Parser::parseIfStmt() {
    panic_on(Token::Type::LPAREN);
    const auto cond = parseCond();
    panic_on(Token::Type::RPAREN);
    const auto then_stmt = parseStmt();
    std::shared_ptr<AST::Stmt> else_stmt = nullptr;
    if (match(Token::Type::ELSE)) {
        else_stmt = parseStmt();
    }
    return std::make_shared<AST::IfStmt>(cond, then_stmt, else_stmt);
}

std::shared_ptr<AST::WhileStmt> Parser::parseWhileStmt() {
    panic_on(Token::Type::LPAREN);
    const auto cond = parseCond();
    panic_on(Token::Type::RPAREN);
    const auto body = parseStmt();
    return std::make_shared<AST::WhileStmt>(cond, body);
}

std::shared_ptr<AST::Exp> Parser::parseExp() {
    if (match(Token::Type::STRING_CONST)) {
        const auto &string_const = next(-1).content;
        return std::make_shared<AST::Exp>(string_const);
    }
    std::shared_ptr<AST::AddExp> addExp = parseAddExp();
    return std::make_shared<AST::Exp>(addExp);
}

std::shared_ptr<AST::Cond> Parser::parseCond() {
    const auto lOrExp = parseLOrExp();
    return std::make_shared<AST::Cond>(lOrExp);
}

std::shared_ptr<AST::LVal> Parser::parseLVal() {
    panic_on(Token::Type::IDENTIFIER);
    const std::string ident = next(-1).content;
    std::vector<std::shared_ptr<AST::Exp>> exps;
    if (match(Token::Type::LBRACKET)) {
        do {
            exps.emplace_back(parseExp());
            panic_on(Token::Type::RBRACKET);
        } while (match(Token::Type::LBRACKET));
    }
    return std::make_shared<AST::LVal>(ident, exps);
}

std::shared_ptr<AST::PrimaryExp> Parser::parsePrimaryExp() {
    if (match(Token::Type::INT_CONST, Token::Type::FLOAT_CONST)) {
        std::shared_ptr<AST::Number> number = parseNumber();
        return std::make_shared<AST::PrimaryExp>(number);
    }
    if (match(Token::Type::LPAREN)) {
        std::shared_ptr<AST::Exp> exp = parseExp();
        panic_on(Token::Type::RPAREN);
        return std::make_shared<AST::PrimaryExp>(exp);
    }
    std::shared_ptr<AST::LVal> lval = parseLVal();
    return std::make_shared<AST::PrimaryExp>(lval);
}

std::shared_ptr<AST::Number> Parser::parseNumber() const {
    if (next(-1).type == Token::Type::INT_CONST) {
        return std::make_shared<AST::IntNumber>(stoi(next(-1).content));
    }
    return std::make_shared<AST::FloatNumber>(next(-1).content);
}

std::shared_ptr<AST::UnaryExp> Parser::parseUnaryExp() {
    if (peek().type == Token::Type::IDENTIFIER && next().type == Token::Type::LPAREN) {
        const auto ident = peek();
        panic_on(Token::Type::IDENTIFIER);
        panic_on(Token::Type::LPAREN);
        // 解析实参列表
        std::vector<std::shared_ptr<AST::Exp>> rParams;
        if (match(Token::Type::RPAREN)) {
            // 实参列表为空
            return std::make_shared<AST::UnaryExp>(ident, rParams);
        }
        do {
            rParams.emplace_back(parseExp());
        } while (match(Token::Type::COMMA));
        panic_on(Token::Type::RPAREN);
        return std::make_shared<AST::UnaryExp>(ident, rParams);
    }
    if (match(Token::Type::ADD, Token::Type::SUB, Token::Type::NOT)) {
        Token::Type type = next(-1).type;
        std::shared_ptr<AST::UnaryExp> unaryExp = parseUnaryExp();
        return std::make_shared<AST::UnaryExp>(type, unaryExp);
    }
    std::shared_ptr<AST::PrimaryExp> primaryExp = parsePrimaryExp();
    return std::make_shared<AST::UnaryExp>(primaryExp);
}

std::shared_ptr<AST::MulExp> Parser::parseMulExp() {
    std::vector<std::shared_ptr<AST::UnaryExp>> unaryExps;
    std::vector<Token::Type> operators;
    unaryExps.emplace_back(parseUnaryExp());
    while (match(Token::Type::MUL, Token::Type::DIV, Token::Type::MOD)) {
        operators.emplace_back(next(-1).type);
        unaryExps.emplace_back(parseUnaryExp());
    }
    return std::make_shared<AST::MulExp>(unaryExps, operators);
}

std::shared_ptr<AST::AddExp> Parser::parseAddExp() {
    std::vector<std::shared_ptr<AST::MulExp>> mulExps;
    std::vector<Token::Type> operators;
    mulExps.emplace_back(parseMulExp());
    while (match(Token::Type::ADD, Token::Type::SUB)) {
        operators.emplace_back(next(-1).type);
        mulExps.emplace_back(parseMulExp());
    }
    return std::make_shared<AST::AddExp>(mulExps, operators);
}

std::shared_ptr<AST::RelExp> Parser::parseRelExp() {
    std::vector<std::shared_ptr<AST::AddExp>> addExps;
    std::vector<Token::Type> operators;
    addExps.emplace_back(parseAddExp());
    while (match(Token::Type::LE, Token::Type::GE, Token::Type::LT, Token::Type::GT)) {
        operators.emplace_back(next(-1).type);
        addExps.emplace_back(parseAddExp());
    }
    return std::make_shared<AST::RelExp>(addExps, operators);
}

std::shared_ptr<AST::EqExp> Parser::parseEqExp() {
    std::vector<std::shared_ptr<AST::RelExp>> relExps;
    std::vector<Token::Type> operators;
    relExps.emplace_back(parseRelExp());
    while (match(Token::Type::EQ, Token::Type::NE)) {
        operators.emplace_back(next(-1).type);
        relExps.emplace_back(parseRelExp());
    }
    return std::make_shared<AST::EqExp>(relExps, operators);
}

std::shared_ptr<AST::LAndExp> Parser::parseLAndExp() {
    std::vector<std::shared_ptr<AST::EqExp>> eqExps;
    do {
        eqExps.emplace_back(parseEqExp());
    } while (match(Token::Type::AND));
    return std::make_shared<AST::LAndExp>(eqExps);
}

std::shared_ptr<AST::LOrExp> Parser::parseLOrExp() {
    std::vector<std::shared_ptr<AST::LAndExp>> lAndExps;
    do {
        lAndExps.emplace_back(parseLAndExp());
    } while (match(Token::Type::OR));
    return std::make_shared<AST::LOrExp>(lAndExps);
}

std::shared_ptr<AST::ConstExp> Parser::parseConstExp() {
    std::shared_ptr<AST::AddExp> addExp = parseAddExp();
    return std::make_shared<AST::ConstExp>(addExp);
}

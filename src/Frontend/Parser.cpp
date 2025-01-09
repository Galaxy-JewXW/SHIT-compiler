#include "Frontend/Parser.h"
#include "Utils/AST.h"

#include <algorithm>

template<typename... Types>
bool Parser::panic_on(Types... expected_types) {
    std::vector<Token::Type> types = {expected_types...};
    const Token::Type current_type = peek().type;
    if (const auto it
            = std::find(types.begin(), types.end(), current_type); it == types.end()) {
        std::cerr << "[Parser] Expected one of {";
        for (const auto &type: types) {
            std::cerr << type_to_string(type) << " ";
        }
        std::cerr << "}, got Token " << type_to_string(current_type) << std::endl;
        throw std::runtime_error("Parser panic");
    }
    pos++;
    return true;
}

template<typename... Types>
bool Parser::match(Types... expected_types) {
    std::vector<Token::Type> types = {expected_types...};
    const Token::Type current_type = peek().type;
    if (const auto it
            = std::find(types.begin(), types.end(), current_type); it == types.end())
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
    // TODO: 函数体解析
    return nullptr;
}

std::shared_ptr<AST::Exp> Parser::parseExp() {
    std::shared_ptr<AST::AddExp> addExp = parseAddExp();
    return std::make_shared<AST::Exp>(addExp);
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
    return std::make_shared<AST::FloatNumber>(stod(next(-1).content));
}

std::shared_ptr<AST::UnaryExp> Parser::parseUnaryExp() {
    if (peek().type == Token::Type::IDENTIFIER && next().type == Token::Type::LPAREN) {
        const std::string ident = peek().content;
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
    if (match(Token::Type::AND, Token::Type::SUB, Token::Type::NOT)) {
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

std::shared_ptr<AST::ConstExp> Parser::parseConstExp() {
    std::shared_ptr<AST::AddExp> addExp = parseAddExp();
    return std::make_shared<AST::ConstExp>(addExp);
}

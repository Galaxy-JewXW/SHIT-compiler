#include "Frontend/Parser.h"
#include "Utils/AST.h"

#include <algorithm>
#include <cassert>

template<typename... Types>
bool Parser::panic_on(Types... expected_types) {
    std::vector<Token::Type> types = {expected_types...};
    const Token::Type current_type = peek().type;
    if (const auto it
                = std::find(types.begin(), types.end(), current_type);
        it == types.end()) {
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
                = std::find(types.begin(), types.end(), current_type);
        it == types.end())
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
    Token::Type bType = peek().type;
    assert(bType == Token::Type::INT || bType == Token::Type::FLOAT);
    std::vector<std::shared_ptr<AST::ConstDef>> constDefs;
    do {
        constDefs.emplace_back(parseConstDef());
    } while (match(Token::Type::COMMA));
    return std::make_shared<AST::ConstDecl>(bType, constDefs);
}

std::shared_ptr<AST::ConstDef> Parser::parseConstDef() {
    panic_on(Token::Type::IDENTIFIER);
}

std::shared_ptr<AST::FuncDef> Parser::parseFuncDef() {
    return nullptr;
}

#ifndef _TOKEN_H_
#define _TOKEN_H_

#include <string>
#include <map>
#include <iostream>

enum TokenType
{
    IDENT,
    ADD,
    SUB,
    NOT,
    MUL,
    DIV,
    MOD,
    LT,
    GT,
    LE,
    GE,
    NE,
    EQ,
    AND,
    OR,
    ASSIGN,
    // TODO
};

class Token
{
private:
    TokenType matchType(std::string);
public:
    Token(std::string, int);
    std::string content;
    int at_line;
    TokenType type;
};

const std::map<std::string, TokenType> tokenMap = {
    {"+", TokenType::ADD},
    {"-", TokenType::SUB},
    {"!", TokenType::NOT},
    {"*", TokenType::MUL},
    {"/", TokenType::DIV},
    {"%", TokenType::MOD},
    {"<", TokenType::LT},
    {">", TokenType::GT},
    {"<=", TokenType::LE},
    {">=", TokenType::GE},
    {"!=", TokenType::NE},
    {"==", TokenType::EQ},
    {"&&", TokenType::AND},
    {"||", TokenType::OR},
    {"=", TokenType::ASSIGN},
    // TODO
};

#endif
#ifndef TOKEN_H
#define TOKEN_H

#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace Token {
enum class Type {
    // 关键词
    CONST, INT, FLOAT, VOID, IF, ELSE, WHILE, BREAK, CONTINUE, RETURN,
    // 标识符
    IDENTIFIER,
    // 字面量
    INT_CONST, FLOAT_CONST, STRING_CONST,
    // 运算符
    ADD, SUB, NOT, MUL, DIV, MOD,
    LT, GT, LE, GE, EQ, NE, AND, OR,
    // 分隔符
    SEMICOLON, COMMA, ASSIGN,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    // 结束符
    END_OF_FILE,
    // 未知
    UNKNOWN
};

std::string type_to_string(Type type);

class Token {
public:
    const std::string content;
    const Type type;
    const int line;

    Token(std::string c, const Type t, const int l)
        : content(std::move(c)), type(t), line(l) {}

    [[nodiscard]] std::string to_string() const;
};
}

#endif

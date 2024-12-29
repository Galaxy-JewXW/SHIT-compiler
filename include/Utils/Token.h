#ifndef TOKEN_H_
#define TOKEN_H_

#include <string>
#include <utility>

enum class TokenType {
    // 关键词
    CONST, INT, FLOAT, VOID, IF, ELSE, WHILE, BREAK, CONTINUE, RETURN, PUTF,
    // 标识符
    IDENTIFIER,
    // 字面量
    INT_CONST, OCT_CONST, HEX_CONST, FLOAT_CONST, STRING_CONST,
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

class Token {
    const std::string content;
    const TokenType type;
    const int line;

    static std::string type_to_string(const TokenType type) {
        switch (type) {
            // 关键词
            case TokenType::CONST: return "CONST";
            case TokenType::INT: return "INT";
            case TokenType::FLOAT: return "FLOAT";
            case TokenType::VOID: return "VOID";
            case TokenType::IF: return "IF";
            case TokenType::ELSE: return "ELSE";
            case TokenType::WHILE: return "WHILE";
            case TokenType::BREAK: return "BREAK";
            case TokenType::CONTINUE: return "CONTINUE";
            case TokenType::RETURN: return "RETURN";
            case TokenType::PUTF: return "PUTF";

            // 标识符
            case TokenType::IDENTIFIER: return "IDENTIFIER";

            // 字面量
            case TokenType::INT_CONST: return "INT_CONST";
            case TokenType::OCT_CONST: return "OCT_CONST";
            case TokenType::HEX_CONST: return "HEX_CONST";
            case TokenType::FLOAT_CONST: return "FLOAT_CONST";
            case TokenType::STRING_CONST: return "STRING_CONST";

            // 运算符
            case TokenType::ADD: return "ADD";
            case TokenType::SUB: return "SUB";
            case TokenType::NOT: return "NOT";
            case TokenType::MUL: return "MUL";
            case TokenType::DIV: return "DIV";
            case TokenType::MOD: return "MOD";
            case TokenType::LT: return "LT";
            case TokenType::GT: return "GT";
            case TokenType::LE: return "LE";
            case TokenType::GE: return "GE";
            case TokenType::EQ: return "EQ";
            case TokenType::NE: return "NE";
            case TokenType::AND: return "AND";
            case TokenType::OR: return "OR";

            // 分隔符
            case TokenType::SEMICOLON: return "SEMICOLON";
            case TokenType::COMMA: return "COMMA";
            case TokenType::ASSIGN: return "ASSIGN";
            case TokenType::LPAREN: return "LPAREN";
            case TokenType::RPAREN: return "RPAREN";
            case TokenType::LBRACE: return "LBRACE";
            case TokenType::RBRACE: return "RBRACE";
            case TokenType::LBRACKET: return "LBRACKET";
            case TokenType::RBRACKET: return "RBRACKET";

            // 结束符
            case TokenType::END_OF_FILE: return "EOF";

            // 未知
            default: return "UNKNOWN";
        }
    }

public:
    Token(std::string c, const TokenType t, const int l)
        : content(std::move(c)), type(t), line(l) {}

    [[nodiscard]] std::string get_content() const {
        return content;
    }

    [[nodiscard]] TokenType get_type() const {
        return type;
    }

    [[nodiscard]] int get_line() const {
        return line;
    }

    void print() const {
        std::cout << "(" << line << ", " << type_to_string(type) << ", " << content << ")\n";
    }
};

#endif

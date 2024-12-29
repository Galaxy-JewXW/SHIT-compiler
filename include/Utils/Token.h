#ifndef TOKEN_H_
#define TOKEN_H_

#include <string>
#include <utility>

namespace Token {
    enum class Type {
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
        const Type type;
        const int line;

        static std::string type_to_string(const Type type) {
            switch (type) {
                // 关键词
                case Type::CONST: return "CONST";
                case Type::INT: return "INT";
                case Type::FLOAT: return "FLOAT";
                case Type::VOID: return "VOID";
                case Type::IF: return "IF";
                case Type::ELSE: return "ELSE";
                case Type::WHILE: return "WHILE";
                case Type::BREAK: return "BREAK";
                case Type::CONTINUE: return "CONTINUE";
                case Type::RETURN: return "RETURN";
                case Type::PUTF: return "PUTF";

                // 标识符
                case Type::IDENTIFIER: return "IDENTIFIER";

                // 字面量
                case Type::INT_CONST: return "INT_CONST";
                case Type::OCT_CONST: return "OCT_CONST";
                case Type::HEX_CONST: return "HEX_CONST";
                case Type::FLOAT_CONST: return "FLOAT_CONST";
                case Type::STRING_CONST: return "STRING_CONST";

                // 运算符
                case Type::ADD: return "ADD";
                case Type::SUB: return "SUB";
                case Type::NOT: return "NOT";
                case Type::MUL: return "MUL";
                case Type::DIV: return "DIV";
                case Type::MOD: return "MOD";
                case Type::LT: return "LT";
                case Type::GT: return "GT";
                case Type::LE: return "LE";
                case Type::GE: return "GE";
                case Type::EQ: return "EQ";
                case Type::NE: return "NE";
                case Type::AND: return "AND";
                case Type::OR: return "OR";

                // 分隔符
                case Type::SEMICOLON: return "SEMICOLON";
                case Type::COMMA: return "COMMA";
                case Type::ASSIGN: return "ASSIGN";
                case Type::LPAREN: return "LPAREN";
                case Type::RPAREN: return "RPAREN";
                case Type::LBRACE: return "LBRACE";
                case Type::RBRACE: return "RBRACE";
                case Type::LBRACKET: return "LBRACKET";
                case Type::RBRACKET: return "RBRACKET";

                // 结束符
                case Type::END_OF_FILE: return "EOF";

                // 未知
                default: return "UNKNOWN";
            }
        }

    public:
        Token(std::string c, const Type t, const int l)
            : content(std::move(c)), type(t), line(l) {}

        [[nodiscard]] std::string get_content() const {
            return content;
        }

        [[nodiscard]] Type get_type() const {
            return type;
        }

        [[nodiscard]] int get_line() const {
            return line;
        }

        void print() const {
            std::cout << "(" << line << ", " << type_to_string(type) << ", " << content << ")\n";
        }
    };
}

#endif

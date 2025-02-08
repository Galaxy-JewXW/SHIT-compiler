#ifndef LEXER_H
#define LEXER_H

#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "../Compiler.h"
#include "../Utils/Token.h"

class Lexer {
    // 以字符串形式保存到源程序
    std::string input;
    // 当前指针位置
    size_t pos;
    // 当前行数
    int line;
    // 解析得到的Token列表
    std::vector<Token::Token> tokens;

    // 关键词集合
    std::unordered_set<std::string> keywords = {
        "const",
        "int",
        "float",
        "void",
        "if",
        "else",
        "while",
        "break",
        "continue",
        "return",
        "putf"
    };

    // 运算符和分隔符映射
    std::unordered_map<std::string, Token::Type> operators = {
        {"+", Token::Type::ADD},
        {"-", Token::Type::SUB},
        {"!", Token::Type::NOT},
        {"*", Token::Type::MUL},
        {"/", Token::Type::DIV},
        {"%", Token::Type::MOD},
        {"<", Token::Type::LT},
        {">", Token::Type::GT},
        {"<=", Token::Type::LE},
        {">=", Token::Type::GE},
        {"==", Token::Type::EQ},
        {"!=", Token::Type::NE},
        {"&&", Token::Type::AND},
        {"||", Token::Type::OR},
        {";", Token::Type::SEMICOLON},
        {",", Token::Type::COMMA},
        {"=", Token::Type::ASSIGN},
        {"(", Token::Type::LPAREN},
        {")", Token::Type::RPAREN},
        {"{", Token::Type::LBRACE},
        {"}", Token::Type::RBRACE},
        {"[", Token::Type::LBRACKET},
        {"]", Token::Type::RBRACKET}
    };

    // 查看当前位置的字符
    char peek() const {
        return input[pos];
    }

    // 查看下一个字符
    char peek_next() const {
        if (pos + 1 >= input.length()) return '\0';
        return input[pos + 1];
    }

    // 消费当前位置的字符并前进
    char advance() {
        const char current = input[pos++];
        if (current == '\n') {
            line++;
        }
        return current;
    }

    // 跳过空白符
    void consume_whitespace() {
        while (pos < input.length() && isspace(peek())) {
            advance();
        }
    }

    // 跳过单行注释
    void consume_line_comment();

    // 跳过多行注释
    void consume_block_comment();

    // 识别标识符或关键词
    Token::Token consume_ident_or_keyword();

    // 识别数字（整数或浮点数）
    Token::Token consume_number();

    // 识别字符串
    Token::Token consume_string();

    // 识别运算符或未知字符
    Token::Token consume_operator();

    // 将关键词字符串转换为TokenType
    static Token::Type string_to_type(const std::string &str) {
        if (str == "const") return Token::Type::CONST;
        if (str == "int") return Token::Type::INT;
        if (str == "float") return Token::Type::FLOAT;
        if (str == "void") return Token::Type::VOID;
        if (str == "if") return Token::Type::IF;
        if (str == "else") return Token::Type::ELSE;
        if (str == "while") return Token::Type::WHILE;
        if (str == "break") return Token::Type::BREAK;
        if (str == "continue") return Token::Type::CONTINUE;
        if (str == "return") return Token::Type::RETURN;
        return Token::Type::IDENTIFIER;
    }

public:
    explicit Lexer(std::string src) : input(std::move(src)), pos(0), line(1) {}

    // 获取分割好的Token列表
    const std::vector<Token::Token> &tokenize();
};

#endif

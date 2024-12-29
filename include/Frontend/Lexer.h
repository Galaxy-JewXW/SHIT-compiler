#ifndef LEXER_H_
#define LEXER_H_

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
    std::vector<Token> tokens;

    // 关键词集合
    std::unordered_set<std::string> keywords = {
        "const", "int", "float", "void", "if", "else", "while",
        "break", "continue", "return", "putf"
    };

    // 运算符和分隔符映射
    std::unordered_map<std::string, TokenType> operators = {
        {"+", TokenType::ADD}, {"-", TokenType::SUB}, {"!", TokenType::NOT},
        {"*", TokenType::MUL}, {"/", TokenType::DIV}, {"%", TokenType::MOD},
        {"<", TokenType::LT}, {">", TokenType::GT}, {"<=", TokenType::LE},
        {">=", TokenType::GE}, {"==", TokenType::EQ}, {"!=", TokenType::NE},
        {"&&", TokenType::AND}, {"||", TokenType::OR},
        {";", TokenType::SEMICOLON}, {",", TokenType::COMMA},
        {"=", TokenType::ASSIGN}, {"(", TokenType::LPAREN},
        {")", TokenType::RPAREN}, {"{", TokenType::LBRACE},
        {"}", TokenType::RBRACE}, {"[", TokenType::LBRACKET},
        {"]", TokenType::RBRACKET}
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
    void consume_line_comment() {
        advance(); // '/'
        advance(); // '/'
        while (pos < input.length() && peek() != '\n') {
            advance();
        }
    }

    // 跳过多行注释
    void consume_block_comment() {
        advance(); // '/'
        advance(); // '*'
        while (pos < input.length()) {
            if (peek() == '*' && peek_next() == '/') {
                advance(); // '*'
                advance(); // '/'
                break;
            }
            advance();
        }
    }

    // 识别标识符或关键词
    Token consume_ident_or_keyword() {
        const int start_line = line;
        std::string lexeme;
        while (pos < input.length() && (isalnum(peek()) || peek() == '_')) {
            lexeme += advance();
        }

        if (keywords.find(lexeme) != keywords.end()) {
            return Token{lexeme, string_to_tokentype(lexeme), start_line};
        }
        return Token{lexeme, TokenType::IDENTIFIER, start_line};
    }

    // 将关键词字符串转换为TokenType
    static TokenType string_to_tokentype(const std::string &str) {
        if (str == "const") return TokenType::CONST;
        if (str == "int") return TokenType::INT;
        if (str == "float") return TokenType::FLOAT;
        if (str == "void") return TokenType::VOID;
        if (str == "if") return TokenType::IF;
        if (str == "else") return TokenType::ELSE;
        if (str == "while") return TokenType::WHILE;
        if (str == "break") return TokenType::BREAK;
        if (str == "continue") return TokenType::CONTINUE;
        if (str == "return") return TokenType::RETURN;
        if (str == "putf") return TokenType::PUTF;
        return TokenType::IDENTIFIER;
    }

    // 识别数字（整数或浮点数）
    Token consume_number() {
        const int start_line = line;
        std::string number;
        bool is_float = false;
        // 检查是否为十六进制数（以 0x 或 0X 开头）
        if (peek() == '0' && (peek_next() == 'x' || peek_next() == 'X')) {
            number += advance();
            number += advance();
            // 解析十六进制数字部分
            while (pos < input.length() && std::isxdigit(peek())) {
                number += advance();
            }
            // 检查是否有小数点 '.'
            if (peek() == '.') {
                is_float = true;
                number += advance(); // 添加 '.'
                while (pos < input.length() && std::isxdigit(peek())) {
                    number += advance();
                }
            }
            // 检查是否有指数部分 'p', 'P', 'e', 'E'
            if (peek() == 'p' || peek() == 'P' || peek() == 'e' || peek() == 'E') {
                is_float = true;
                number += advance();
                // 处理指数的可选符号 '+' 或 '-'
                if (peek() == '+' || peek() == '-') {
                    number += advance();
                }
                // 对于十六进制浮点数，指数部分可以包含十六进制数字
                while (pos < input.length() && std::isxdigit(peek())) {
                    number += advance();
                }
            }
            if (is_float) {
                return Token{number, TokenType::FLOAT_CONST, start_line};
            }
            return Token{number, TokenType::HEX_CONST, start_line};
        }
        // 八进制数
        if (peek() == '0' && std::isdigit(peek_next())) {
            number += advance(); // 添加 '0'
            while (pos < input.length() && peek() >= '0' && peek() <= '7') {
                number += advance();
            }
            return Token{number, TokenType::OCT_CONST, start_line};
        }
        // 十进制数字
        while (pos < input.length() && std::isdigit(peek())) {
            number += advance();
        }
        // 检查是否有小数点 '.'
        if (peek() == '.') {
            is_float = true;
            number += advance(); // 添加 '.'
            while (pos < input.length() && std::isdigit(peek())) {
                number += advance();
            }
        }
        // 检查是否有指数部分 'e', 'E', 'p', 'P'
        if (peek() == 'e' || peek() == 'E' || peek() == 'p' || peek() == 'P') {
            is_float = true;
            number += advance();
            // 可选符号 '+' 或 '-'
            if (peek() == '+' || peek() == '-') {
                number += advance();
            }
            // 十进制浮点数指数部分仅包含十进制数字
            while (pos < input.length() && std::isdigit(peek())) {
                number += advance();
            }
        }
        // 检查是否有浮点数后缀 'f', 'F', 'l', 'L'
        if (peek() == 'f' || peek() == 'F' || peek() == 'l' || peek() == 'L') {
            is_float = true;
            number += advance(); // 添加后缀
        }
        if (is_float) {
            return Token{number, TokenType::FLOAT_CONST, start_line};
        }
        return Token{number, TokenType::INT_CONST, start_line};
    }

    // 识别字符串
    Token consume_string() {
        const int start_line = line;
        std::string str;
        advance(); // 消费第一个双引号
        while (pos < input.length()) {
            if (const char current = peek(); current == '\\') {
                // 转义字符
                str += advance(); // '\\'
                if (pos < input.length()) {
                    str += advance(); // 转义后的字符
                }
            } else if (current == '"') {
                // 结束双引号
                advance();
                break;
            } else {
                str += advance();
            }
        }
        return Token{str, TokenType::STRING_CONST, start_line};
    }

    // 识别运算符或未知字符
    Token consume_operator() {
        const int start_line = line;
        std::string op;
        op += advance();

        // 尝试匹配两个字符的运算符
        if (pos < input.length()) {
            if (const std::string twoCharOp = op + std::string(1, peek());
                operators.find(twoCharOp) != operators.end()) {
                advance();
                return Token{twoCharOp, operators[twoCharOp], start_line};
            }
        }

        // 尝试匹配单字符运算符
        if (operators.find(op) != operators.end()) {
            return Token{op, operators[op], start_line};
        }

        // 未知字符
        return Token{op, TokenType::UNKNOWN, start_line};
    }

public:
    explicit Lexer(std::string src) : input(std::move(src)), pos(0), line(1) {}

    // 获取分割好的Token列表
    std::vector<Token> tokenize();
};

#endif

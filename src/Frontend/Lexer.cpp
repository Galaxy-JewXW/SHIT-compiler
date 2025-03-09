#include <iomanip>

#include "Frontend/Lexer.h"
#include "Utils/Log.h"

void Lexer::consume_line_comment() {
    advance(); // '/'
    advance(); // '/'
    while (pos < input.length() && peek() != '\n') {
        advance();
    }
}

void Lexer::consume_block_comment() {
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

Token::Token Lexer::consume_ident_or_keyword() {
    const int start_line = line;
    std::string lexeme;
    while (pos < input.length() && (isalnum(peek()) || peek() == '_')) {
        lexeme += advance();
    }

    if (keywords.find(lexeme) != keywords.end()) {
        return Token::Token{lexeme, string_to_type(lexeme), start_line};
    }
    return Token::Token{lexeme, Token::Type::IDENTIFIER, start_line};
}

Token::Token Lexer::consume_number() {
    const int start_line = line;
    std::string number;
    bool is_float = false;
    std::size_t idx;

    // 十六进制数处理 (0x...)
    if (peek() == '0' && (peek_next() == 'x' || peek_next() == 'X')) {
        number += advance(); // '0'
        number += advance(); // 'x'/'X'
        while (pos < input.length() && std::isxdigit(peek())) {
            number += advance();
        }

        // 处理十六进制浮点数
        if (peek() == '.' || peek() == 'p' || peek() == 'P') {
            if (peek() == '.') {
                number += advance(); // '.'
                while (pos < input.length() && std::isxdigit(peek())) {
                    number += advance();
                }
            }
            if (peek() == 'p' || peek() == 'P') {
                number += advance(); // 'p'/'P'
                if (peek() == '+' || peek() == '-') number += advance();
                while (pos < input.length() && std::isdigit(peek())) {
                    number += advance();
                }
            }
            return Token::Token{
                std::to_string(std::stod(number, &idx)),
                Token::Type::FLOAT_CONST, start_line
            };
        }
        return Token::Token{
            std::to_string(std::stoi(number, &idx, 16)),
            Token::Type::INT_CONST, start_line
        };
    }

    // 八进制数处理 (0后面紧跟0-7)
    if (peek() == '0' && (peek_next() >= '0' && peek_next() <= '7')) {
        number += advance(); // '0'
        while (pos < input.length() && peek() >= '0' && peek() <= '7') {
            number += advance();
        }

        // 检查八进制后是否接浮点或指数（转换为十进制处理）
        if (peek() == '.' || peek() == 'e' || peek() == 'E') {
            int oct_part = std::stoi(number, nullptr, 8);
            number = std::to_string(oct_part);
            is_float = true;
        } else {
            return Token::Token{
                std::to_string(std::stoi(number, &idx, 8)),
                Token::Type::INT_CONST, start_line
            };
        }
    }

    // 十进制数字处理
    if (number.empty()) {
        while (pos < input.length() && std::isdigit(peek())) {
            number += advance();
        }
    }

    // 前导零错误检查（仅限十进制整数）
    if (!is_float && number.size() > 1 && number[0] == '0') {
        log_fatal("Invalid leading zero in integer at line %d", start_line);
    }

    // 浮点数处理
    if (peek() == '.') {
        is_float = true;
        number += advance();
        while (pos < input.length() && std::isdigit(peek())) {
            number += advance();
        }
    }

    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        number += advance();
        if (peek() == '+' || peek() == '-') number += advance();
        while (pos < input.length() && std::isdigit(peek())) {
            number += advance();
        }
    }

    // 浮点后缀
    if (peek() == 'f' || peek() == 'F' || peek() == 'l' || peek() == 'L') {
        is_float = true;
        number += advance();
    }

    if (is_float) {
        std::ostringstream oss;
        double val = std::stod(number, &idx);
        oss << std::setprecision(12) << val;
        return Token::Token{oss.str(), Token::Type::FLOAT_CONST, start_line};
    }

    return Token::Token{
        std::to_string(std::stoi(number, &idx, 10)),
        Token::Type::INT_CONST, start_line
    };
}

Token::Token Lexer::consume_string() {
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
    return Token::Token{str, Token::Type::STRING_CONST, start_line};
}

// 识别运算符或未知字符
Token::Token Lexer::consume_operator() {
    const int start_line = line;
    std::string op;
    op += advance();

    // 尝试匹配两个字符的运算符
    if (pos < input.length()) {
        if (const std::string two_char_op = op + std::string(1, peek());
            operators.find(two_char_op) != operators.end()) {
            advance();
            return Token::Token{two_char_op, operators[two_char_op], start_line};
        }
    }

    // 尝试匹配单字符运算符
    if (operators.find(op) != operators.end()) {
        return Token::Token{op, operators[op], start_line};
    }

    // 未知字符
    log_fatal("Unrecognized operator %s at line %d", op.c_str(), start_line);
}

const std::vector<Token::Token> &Lexer::tokenize() {
    while (pos < input.length()) {
        const char current = peek();
        // 跳过空白符和注释
        if (isspace(current)) {
            consume_whitespace();
            continue;
        }
        if (current == '/') {
            if (peek_next() == '/') {
                consume_line_comment();
                continue;
            }
            if (peek_next() == '*') {
                consume_block_comment();
                continue;
            }
        }
        // 识别Token
        if (isalpha(current) || current == '_') {
            tokens.push_back(consume_ident_or_keyword());
        } else if (isdigit(current) || current == '.') {
            tokens.push_back(consume_number());
        } else if (current == '"') {
            tokens.push_back(consume_string());
        } else {
            tokens.push_back(consume_operator());
        }
    }
    tokens.emplace_back("\0", Token::Type::END_OF_FILE, line);
    return tokens;
}

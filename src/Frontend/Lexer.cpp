#include "Frontend/Lexer.h"

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
        return Token::Token{lexeme, string_to_tokentype(lexeme), start_line};
    }
    return Token::Token{lexeme, Token::Type::IDENTIFIER, start_line};
}

Token::Token Lexer::consume_number() {
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
            return Token::Token{number, Token::Type::FLOAT_CONST, start_line};
        }
        return Token::Token{number, Token::Type::HEX_CONST, start_line};
    }
    // 八进制数
    if (peek() == '0' && std::isdigit(peek_next())) {
        number += advance(); // 添加 '0'
        while (pos < input.length() && peek() >= '0' && peek() <= '7') {
            number += advance();
        }
        return Token::Token{number, Token::Type::OCT_CONST, start_line};
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
        return Token::Token{number, Token::Type::FLOAT_CONST, start_line};
    }
    return Token::Token{number, Token::Type::INT_CONST, start_line};
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
    return Token::Token{op, Token::Type::UNKNOWN, start_line};
}

std::vector<Token::Token> Lexer::tokenize() {
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
        } else if (isdigit(current)) {
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

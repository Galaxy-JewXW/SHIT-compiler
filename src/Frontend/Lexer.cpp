#include "Frontend/Lexer.h"

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

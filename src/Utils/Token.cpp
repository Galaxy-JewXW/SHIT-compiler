#include "Utils/Token.h"

Token::Token(std::string content, int at_line) {
    this->content = content;
    this->at_line = at_line;
    this->type = matchType(content);
}

TokenType Token::matchType(std::string content) {
    if (tokenMap.find(content) != tokenMap.end()) {
        return tokenMap.at(content);
    } return TokenType::IDENT;
}
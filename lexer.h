#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "token.hpp"

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    std::string src_;
    std::size_t pos_ = 0;
    int line_ = 1;

    bool atEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    void skipWhitespaceAndComments();
    Token makeToken(TokType type, std::string text) const;
    Token identifier();
    Token number();
    Token stringLiteral(char quote);
    Token symbol(TokType type, std::string text) const;
};

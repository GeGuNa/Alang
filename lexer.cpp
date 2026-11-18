#include "lexer.h"

#include <cctype>
#include <unordered_map>
#include <utility>

const std::unordered_map<std::string, TokType>& keywords() {
    static const std::unordered_map<std::string, TokType> table = {
        {"int", T_INT_TYPE},
        {"float", T_FLOAT_TYPE},
        {"double", T_DOUBLE_TYPE},
        {"string", T_STRING_TYPE},
        {"str", T_STR_TYPE},
        {"void", T_VOID},
        {"bool", T_BOOL},
        {"char", T_CHAR_TYPE},
        {"fn", T_FN},
        {"class", T_CLASS},
        {"init", T_INIT},
        {"deinit", T_DEINIT},
        {"this", T_THIS},
        {"new", T_NEW},
        {"null", T_NULL},
        {"if", T_IF},
        {"else", T_ELSE},
        {"for", T_FOR},
        {"while", T_WHILE},
        {"switch", T_SWITCH},
        {"case", T_CASE},
        {"default", T_DEFAULT},
        {"break", T_BREAK},
        {"continue", T_CONTINUE},
        {"return", T_RETURN},
        {"const", T_CONST},
        {"var", T_VAR},
        {"function", T_FUNCTION},
        {"thread", T_THREAD},
        {"async", T_ASYNC},
        {"await", T_AWAIT},
        {"extends", T_EXTENDS},
        {"extend", T_EXTEND},
        {"namespace", T_NAMESPACE},
        {"using", T_USING},
        {"parent", T_PARENT},
        {"public", T_PUBLIC},
        {"private", T_PRIVATE},
        {"protected", T_PROTECTED},
        {"static", T_STATIC},
        {"true", T_BOOL_LIT},
        {"false", T_BOOL_LIT}
    };
    return table;
}

bool isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool isAlpha(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool isAlnum(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

Lexer::Lexer(std::string source) : src_(std::move(source)) {}

bool Lexer::atEnd() const {
    return pos_ >= src_.size();
}

char Lexer::peek() const {
    return atEnd() ? '\0' : src_[pos_];
}

char Lexer::peekNext() const {
    return pos_ + 1 >= src_.size() ? '\0' : src_[pos_ + 1];
}

char Lexer::advance() {
    if (atEnd()) {
        return '\0';
    }
    return src_[pos_++];
}

bool Lexer::match(char expected) {
    if (atEnd() || src_[pos_] != expected) {
        return false;
    }
    ++pos_;
    return true;
}

Token Lexer::makeToken(TokType type, std::string text) const {
    Token token;
    token.type = type;
    token.text = std::move(text);
    token.line = line_;
    return token;
}

Token Lexer::symbol(TokType type, std::string text) const {
    return makeToken(type, std::move(text));
}

void Lexer::skipWhitespaceAndComments() {
    while (!atEnd()) {
        char c = peek();

        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            if (c == '\n') {
                ++line_;
            }
            advance();
            continue;
        }

        if (c == '/' && peekNext() == '/') {
            while (!atEnd() && peek() != '\n') {
                advance();
            }
            continue;
        }

        if (c == '#') {
            while (!atEnd() && peek() != '\n') {
                advance();
            }
            continue;
        }

        if (c == '/' && peekNext() == '*') {
            advance();
            advance();
            while (!atEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance();
                    advance();
                    break;
                }
                if (peek() == '\n') {
                    ++line_;
                }
                advance();
            }
            continue;
        }

        break;
    }
}

Token Lexer::identifier() {
    std::size_t start = pos_;
    while (!atEnd() && (isAlnum(peek()) || peek() == '_')) {
        advance();
    }

    std::string text = src_.substr(start, pos_ - start);
    auto it = keywords().find(text);
    if (it == keywords().end()) {
        return makeToken(T_IDENT, text);
    }

    Token token = makeToken(it->second, text);
    token.bval = (text == "true");
    return token;
}

Token Lexer::number() {
    std::size_t start = pos_;
    while (!atEnd() && isDigit(peek())) {
        advance();
    }

    bool isFloat = false;
    if (peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance();
        while (!atEnd() && isDigit(peek())) {
            advance();
        }
    }

    std::string text = src_.substr(start, pos_ - start);
    Token token = makeToken(isFloat ? T_FLOAT_LIT : T_INT_LIT, text);
    if (isFloat) {
        token.dval = std::stod(text);
    } else {
        token.ival = std::stoll(text);
    }
    return token;
}

Token Lexer::stringLiteral(char quote) {
    advance();
    std::string value;
    bool closed = false;

    while (!atEnd()) {
        char c = advance();

        if (c == quote) {
            closed = true;
            break;
        }

        if (c == '\n') {
            ++line_;
        }

        if (c == '\\' && !atEnd()) {
            char escaped = advance();
            switch (escaped) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case 'r': value.push_back('\r'); break;
                case '0': value.push_back('\0'); break;
                case '\\': value.push_back('\\'); break;
                case '\'': value.push_back('\''); break;
                case '"': value.push_back('"'); break;
                default: value.push_back(escaped); break;
            }
        } else {
            value.push_back(c);
        }
    }

    if (!closed) {
        return makeToken(T_ERROR, value);
    }
    return makeToken(quote == '\'' ? T_CHAR_LIT : T_STRING_LIT, value);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!atEnd()) {
        skipWhitespaceAndComments();
        if (atEnd()) {
            break;
        }

        char c = peek();

        if (c == '$') {
            advance();
            if (!atEnd() && (isAlpha(peek()) || peek() == '_')) {
                Token name = identifier();
                name.text = "$" + name.text;
                name.dollar = true;
                tokens.push_back(name);
            } else {
                tokens.push_back(symbol(T_ERROR, "$"));
            }
            continue;
        }

        if (isAlpha(c) || c == '_') {
            tokens.push_back(identifier());
            continue;
        }

        if (isDigit(c)) {
            tokens.push_back(number());
            continue;
        }

        if (c == '"' || c == '\'') {
            tokens.push_back(stringLiteral(c));
            continue;
        }

        TokType type = T_ERROR;
        std::string text;

        switch (c) {
            case '=':
                if (peekNext() == '=') {
                    advance();
                    advance();
                    type = T_EQ;
                    text = "==";
                } else {
                    advance();
                    type = T_ASSIGN;
                    text = "=";
                }
                break;
            case '!':
                if (peekNext() == '=') {
                    advance();
                    advance();
                    type = T_NE;
                    text = "!=";
                } else {
                    advance();
                    type = T_NOT;
                    text = "!";
                }
                break;
            case '>':
                if (peekNext() == '=') {
                    advance();
                    advance();
                    type = T_GE;
                    text = ">=";
                } else {
                    advance();
                    type = T_GT;
                    text = ">";
                }
                break;
            case '<':
                if (peekNext() == '=') {
                    advance();
                    advance();
                    type = T_LE;
                    text = "<=";
                } else {
                    advance();
                    type = T_LT;
                    text = "<";
                }
                break;
            case '&':
                if (peekNext() == '&') {
                    advance();
                    advance();
                    type = T_AND;
                    text = "&&";
                } else {
                    advance();
                    text = "&";
                }
                break;
            case '|':
                if (peekNext() == '|') {
                    advance();
                    advance();
                    type = T_OR;
                    text = "||";
                } else {
                    advance();
                    text = "|";
                }
                break;
            case '+': advance(); type = T_PLUS; text = "+"; break;
            case '-':
                if (peekNext() == '>') {
                    advance();
                    advance();
                    type = T_ARROW;
                    text = "->";
                } else {
                    advance();
                    type = T_MINUS;
                    text = "-";
                }
                break;
            case ':':
                if (peekNext() == ':') {
                    advance();
                    advance();
                    type = T_DOUBLE_COLON;
                    text = "::";
                } else {
                    advance();
                    type = T_COLON;
                    text = ":";
                }
                break;
            case '*': advance(); type = T_STAR; text = "*"; break;
            case '/': advance(); type = T_SLASH; text = "/"; break;
            case '%': advance(); type = T_PERCENT; text = "%"; break;
            case ';': advance(); type = T_SEMICOLON; text = ";"; break;
            case ',': advance(); type = T_COMMA; text = ","; break;
            case '(': advance(); type = T_LPAREN; text = "("; break;
            case ')': advance(); type = T_RPAREN; text = ")"; break;
            case '{': advance(); type = T_LBRACE; text = "{"; break;
            case '}': advance(); type = T_RBRACE; text = "}"; break;
            case '.': advance(); type = T_DOT; text = "."; break;
            default: advance(); text = std::string(1, c); break;
        }

        tokens.push_back(symbol(type, text));
    }

    Token end;
    end.type = T_EOF;
    end.line = line_;
    tokens.push_back(end);
    return tokens;
}

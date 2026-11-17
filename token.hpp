#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

enum TokType {
    T_EOF,
    T_IDENT,
    T_INT_LIT,
    T_FLOAT_LIT,
    T_STRING_LIT,
    T_CHAR_LIT,
    T_BOOL_LIT,
    T_FN,
    T_CLASS,
    T_INIT,
    T_DEINIT,
    T_THIS,
    T_NEW,
    T_IF,
    T_ELSE,
    T_FOR,
    T_WHILE,
    T_SWITCH,
    T_CASE,
    T_DEFAULT,
    T_BREAK,
    T_CONTINUE,
    T_RETURN,
    T_CONST,
    T_VAR,
    T_FUNCTION,
    T_VOID,
    T_BOOL,
    T_CHAR_TYPE,
    T_INT_TYPE,
    T_FLOAT_TYPE,
    T_DOUBLE_TYPE,
    T_STRING_TYPE,
    T_STR_TYPE,
    T_ASSIGN,
    T_PLUS,
    T_MINUS,
    T_STAR,
    T_SLASH,
    T_PERCENT,
    T_EQ,
    T_NE,
    T_GT,
    T_GE,
    T_LT,
    T_LE,
    T_NOT,
    T_AND,
    T_OR,
    T_SEMICOLON,
    T_COMMA,
    T_LPAREN,
    T_RPAREN,
    T_LBRACE,
    T_RBRACE,
    T_DOT,
    T_ERROR
};

struct Token {
    TokType type = T_EOF;
    std::string text;
    int line = 1;
    long long ival = 0;
    double dval = 0.0;
    bool bval = false;
    bool dollar = false;
};

enum class ValueType { Void, Int, Double, Bool, String, Function };

struct FunctionValue;

struct Value {
    ValueType type = ValueType::Void;
    long long ival = 0;
    double dval = 0.0;
    bool bval = false;
    std::string sval;
    std::shared_ptr<FunctionValue> fval;
};

inline Value makeVoid() {
    return Value{};
}

inline Value makeInt(long long value) {
    Value result;
    result.type = ValueType::Int;
    result.ival = value;
    return result;
}

inline Value makeDouble(double value) {
    Value result;
    result.type = ValueType::Double;
    result.dval = value;
    return result;
}

inline Value makeBool(bool value) {
    Value result;
    result.type = ValueType::Bool;
    result.bval = value;
    return result;
}

inline Value makeString(std::string value) {
    Value result;
    result.type = ValueType::String;
    result.sval = std::move(value);
    return result;
}

struct LangError : std::runtime_error {
    explicit LangError(const std::string& message) : std::runtime_error(message) {}
};

inline std::string lineMessage(const Token& token, const std::string& message) {
    return "line " + std::to_string(token.line) + ": " + message;
}

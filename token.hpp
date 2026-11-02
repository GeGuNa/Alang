#pragma once

#include <string>
#include <variant>

enum TokType {
    T_INT_LIT, T_FLOAT_LIT, T_STRING_LIT, T_CHAR_LIT, T_BOOL_LIT,
    T_FN, T_CLASS, T_INIT, T_DEINIT, T_THIS, T_NEW,
    T_IF, T_ELSE, T_FOR, T_WHILE, T_SWITCH, T_CASE, T_DEFAULT,
    T_BREAK, T_CONTINUE, T_RETURN,
    T_EOF, T_IDENT
};

struct Token {
    int type;
    std::string text;
    int line;
    long ival;
    double dval;
    bool bval;
};

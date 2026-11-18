#pragma once

#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "ast.h"
#include "token.hpp"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    std::vector<StmtPtr> parse();

private:
    std::vector<Token> tokens_;
    std::size_t current_ = 0;

    const Token& peek() const;
    const Token& previous() const;
    bool atEnd() const;
    bool check(TokType type) const;
    bool match(std::initializer_list<TokType> types);
    const Token& consume(TokType type, const std::string& message);
    void consumeSemicolon();
    bool isTypeToken(TokType type) const;
    bool isFunctionAhead() const;

    std::vector<std::pair<TokType, std::string>> parameterList();

    StmtPtr declaration();
    StmtPtr classDeclaration();
    StmtPtr namespaceDeclaration();
    StmtPtr usingNamespaceStatement();
    StmtPtr functionDeclaration(TokType returnType, bool isAsync = false);
    StmtPtr varDeclaration(TokType type, bool isConst = false, bool isVar = false);
    StmtPtr statement();
    BlockPtr blockStatement();
    StmtPtr ifStatement();
    StmtPtr whileStatement();
    StmtPtr forStatement();
    StmtPtr returnStatement();
    StmtPtr expressionStatement();

    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr logicalOr();
    ExprPtr logicalAnd();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr concat();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr call();
    ExprPtr primary();
    ExprPtr functionExpression();
};

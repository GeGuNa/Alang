#include "parser.h"

#include <memory>
#include <utility>

const Token& Parser::peek() const {
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

bool Parser::atEnd() const {
    return peek().type == T_EOF;
}

bool Parser::check(TokType type) const {
    return peek().type == type;
}

bool Parser::match(std::initializer_list<TokType> types) {
    for (TokType type : types) {
        if (check(type)) {
            ++current_;
            return true;
        }
    }
    return false;
}

const Token& Parser::consume(TokType type, const std::string& message) {
    if (check(type)) {
        return tokens_[current_++];
    }
    throw LangError(lineMessage(peek(), message));
}

void Parser::consumeSemicolon() {
    if (check(T_SEMICOLON)) {
        ++current_;
    }
}

bool Parser::isTypeToken(TokType type) const {
    return type == T_VOID || type == T_BOOL || type == T_CHAR_TYPE ||
           type == T_INT_TYPE || type == T_FLOAT_TYPE || type == T_DOUBLE_TYPE ||
           type == T_STRING_TYPE || type == T_STR_TYPE;
}

bool Parser::isFunctionAhead() const {
    std::size_t i = current_;
    if (i >= tokens_.size()) {
        return false;
    }
    if (tokens_[i].type == T_VOID) {
        return true;
    }
    if (!isTypeToken(tokens_[i].type)) {
        return false;
    }
    ++i;
    if (i >= tokens_.size() || tokens_[i].type != T_IDENT) {
        return false;
    }
    ++i;
    return i < tokens_.size() && tokens_[i].type == T_LPAREN;
}

std::vector<StmtPtr> Parser::parse() {
    std::vector<StmtPtr> statements;
    while (!atEnd()) {
        if (check(T_ERROR)) {
            throw LangError(lineMessage(peek(), "Unexpected character: " + peek().text));
        }
        statements.push_back(declaration());
    }
    return statements;
}

StmtPtr Parser::declaration() {
    if (match({T_FN})) {
        return functionDeclaration(T_VOID);
    }
    if (check(T_FUNCTION) && current_ + 1 < tokens_.size() &&
        tokens_[current_ + 1].type == T_IDENT) {
        ++current_;
        return functionDeclaration(T_VOID);
    }
    if (match({T_CONST})) {
        TokType type = T_VOID;
        if (isTypeToken(peek().type)) {
            type = peek().type;
            ++current_;
        }
        return varDeclaration(type, true, false);
    }
    if (match({T_VAR})) {
        return varDeclaration(T_VOID, false, true);
    }
    if (isFunctionAhead()) {
        TokType returnType = peek().type;
        ++current_;
        return functionDeclaration(returnType);
    }
    if (isTypeToken(peek().type)) {
        TokType type = peek().type;
        ++current_;
        return varDeclaration(type);
    }
    return statement();
}

std::vector<std::pair<TokType, std::string>> Parser::parameterList() {
    consume(T_LPAREN, "Expected '(' after function name");
    std::vector<std::pair<TokType, std::string>> parameters;
    if (!check(T_RPAREN)) {
        do {
            if (check(T_IDENT) && peek().dollar) {
                parameters.emplace_back(T_VOID, peek().text);
                ++current_;
                continue;
            }
            if (!isTypeToken(peek().type)) {
                throw LangError(lineMessage(peek(), "Expected parameter type"));
            }
            TokType parameterType = peek().type;
            ++current_;
            const Token& parameterName = consume(T_IDENT, "Expected parameter name");
            parameters.emplace_back(parameterType, parameterName.text);
        } while (match({T_COMMA}));
    }
    consume(T_RPAREN, "Expected ')' after parameters");
    return parameters;
}

StmtPtr Parser::functionDeclaration(TokType returnType) {
    (void)returnType;
    const Token& nameToken = consume(T_IDENT, "Expected function name");
    std::vector<std::pair<TokType, std::string>> parameters = parameterList();

    BlockPtr body = blockStatement();
    auto function = std::make_shared<FunctionDeclStmt>();
    function->name = nameToken.text;
    function->parameters = std::move(parameters);
    function->body = std::move(body);
    return function;
}

StmtPtr Parser::varDeclaration(TokType type, bool isConst, bool isVar) {
    const Token& nameToken = consume(T_IDENT, "Expected variable name");
    ExprPtr initializer;
    if (match({T_ASSIGN})) {
        initializer = expression();
    } else if (isConst) {
        throw LangError(lineMessage(peek(), "Constant must be initialized: " + nameToken.text));
    } else if (isVar) {
        throw LangError(lineMessage(peek(), "Variable with var must be initialized: " + nameToken.text));
    }
    consumeSemicolon();

    auto declarationNode = std::make_shared<VarDeclStmt>();
    declarationNode->declaredType = type;
    declarationNode->name = nameToken.text;
    declarationNode->initializer = std::move(initializer);
    declarationNode->isConst = isConst;
    declarationNode->isVar = isVar;
    return declarationNode;
}

StmtPtr Parser::statement() {
    if (match({T_IF})) {
        return ifStatement();
    }
    if (match({T_WHILE})) {
        return whileStatement();
    }
    if (match({T_FOR})) {
        return forStatement();
    }
    if (match({T_RETURN})) {
        return returnStatement();
    }
    if (match({T_BREAK})) {
        consumeSemicolon();
        return std::make_shared<BreakStmt>();
    }
    if (match({T_CONTINUE})) {
        consumeSemicolon();
        return std::make_shared<ContinueStmt>();
    }
    if (check(T_LBRACE)) {
        return blockStatement();
    }
    if (match({T_SEMICOLON})) {
        return std::make_shared<EmptyStmt>();
    }
    return expressionStatement();
}

BlockPtr Parser::blockStatement() {
    auto block = std::make_shared<BlockStmt>();
    consume(T_LBRACE, "Expected '{'");
    while (!check(T_RBRACE) && !atEnd()) {
        if (check(T_ERROR)) {
            throw LangError(lineMessage(peek(), "Unexpected character: " + peek().text));
        }
        block->statements.push_back(declaration());
    }
    consume(T_RBRACE, "Expected '}'");
    return block;
}

StmtPtr Parser::ifStatement() {
    consume(T_LPAREN, "Expected '(' after if");
    ExprPtr condition = expression();
    consume(T_RPAREN, "Expected ')' after if condition");
    StmtPtr thenBranch = statement();
    StmtPtr elseBranch;
    if (match({T_ELSE})) {
        elseBranch = statement();
    }

    auto node = std::make_shared<IfStmt>();
    node->condition = std::move(condition);
    node->thenBranch = std::move(thenBranch);
    node->elseBranch = std::move(elseBranch);
    return node;
}

StmtPtr Parser::whileStatement() {
    ExprPtr condition;
    if (check(T_LBRACE)) {
        condition = nullptr;
    } else if (check(T_LPAREN)) {
        ++current_;
        condition = expression();
        consume(T_RPAREN, "Expected ')' after while condition");
    } else {
        condition = expression();
    }
    StmtPtr body = statement();

    auto node = std::make_shared<WhileStmt>();
    node->condition = std::move(condition);
    node->body = std::move(body);
    return node;
}

StmtPtr Parser::forStatement() {
    StmtPtr initializer;
    ExprPtr condition;
    ExprPtr increment;

    if (!check(T_LBRACE) && !check(T_LPAREN)) {
        condition = expression();
    } else if (check(T_LPAREN)) {
        ++current_;
        if (match({T_SEMICOLON})) {
            initializer = std::make_shared<EmptyStmt>();
        } else if (isTypeToken(peek().type)) {
            TokType type = peek().type;
            ++current_;
            initializer = varDeclaration(type);
        } else {
            initializer = expressionStatement();
        }

        if (!check(T_SEMICOLON)) {
            condition = expression();
        }
        consume(T_SEMICOLON, "Expected ';' after for condition");

        if (!check(T_RPAREN)) {
            increment = expression();
        }
        consume(T_RPAREN, "Expected ')' after for clauses");
    }

    StmtPtr body = statement();

    auto node = std::make_shared<ForStmt>();
    node->initializer = std::move(initializer);
    node->condition = std::move(condition);
    node->increment = std::move(increment);
    node->body = std::move(body);
    return node;
}

StmtPtr Parser::returnStatement() {
    ExprPtr value;
    if (!check(T_SEMICOLON) && !check(T_RBRACE) && !atEnd()) {
        value = expression();
    }
    consumeSemicolon();
    return std::make_shared<ReturnStmt>(std::move(value));
}

StmtPtr Parser::expressionStatement() {
    ExprPtr expressionNode = expression();
    consumeSemicolon();
    return std::make_shared<ExprStmt>(std::move(expressionNode));
}

ExprPtr Parser::expression() {
    return assignment();
}

ExprPtr Parser::logicalOr() {
    ExprPtr expr = logicalAnd();
    while (match({T_OR})) {
        TokType op = previous().type;
        expr = std::make_shared<LogicalExpr>(op, expr, logicalAnd());
    }
    return expr;
}

ExprPtr Parser::logicalAnd() {
    ExprPtr expr = equality();
    while (match({T_AND})) {
        TokType op = previous().type;
        expr = std::make_shared<LogicalExpr>(op, expr, equality());
    }
    return expr;
}

ExprPtr Parser::assignment() {
    ExprPtr left = logicalOr();
    if (match({T_ASSIGN})) {
        ExprPtr value = assignment();
        auto variable = std::dynamic_pointer_cast<VarExpr>(left);
        if (!variable) {
            throw LangError("Invalid assignment target");
        }
        return std::make_shared<AssignExpr>(variable->name, std::move(value), variable->fromDollar);
    }
    return left;
}

ExprPtr Parser::equality() {
    ExprPtr expr = comparison();
    while (match({T_EQ, T_NE})) {
        TokType op = previous().type;
        expr = std::make_shared<BinaryExpr>(op, expr, comparison());
    }
    return expr;
}

ExprPtr Parser::comparison() {
    ExprPtr expr = concat();
    while (match({T_GT, T_GE, T_LT, T_LE})) {
        TokType op = previous().type;
        expr = std::make_shared<BinaryExpr>(op, expr, concat());
    }
    return expr;
}

ExprPtr Parser::concat() {
    ExprPtr expr = term();
    while (match({T_DOT})) {
        expr = std::make_shared<BinaryExpr>(T_DOT, expr, term());
    }
    return expr;
}

ExprPtr Parser::term() {
    ExprPtr expr = factor();
    while (match({T_PLUS, T_MINUS})) {
        TokType op = previous().type;
        expr = std::make_shared<BinaryExpr>(op, expr, factor());
    }
    return expr;
}

ExprPtr Parser::factor() {
    ExprPtr expr = unary();
    while (match({T_STAR, T_SLASH, T_PERCENT})) {
        TokType op = previous().type;
        expr = std::make_shared<BinaryExpr>(op, expr, unary());
    }
    return expr;
}

ExprPtr Parser::unary() {
    if (match({T_NOT, T_MINUS})) {
        TokType op = previous().type;
        return std::make_shared<UnaryExpr>(op, unary());
    }
    return call();
}

ExprPtr Parser::call() {
    ExprPtr expr = primary();

    while (match({T_LPAREN})) {
        std::vector<ExprPtr> args;
        if (!check(T_RPAREN)) {
            do {
                args.push_back(expression());
            } while (match({T_COMMA}));
        }
        consume(T_RPAREN, "Expected ')' after arguments");
        expr = std::make_shared<CallExpr>(expr, std::move(args));
    }
    return expr;
}

ExprPtr Parser::functionExpression() {
    std::vector<std::pair<TokType, std::string>> parsed = parameterList();
    std::vector<std::string> parameters;
    for (const auto& parameter : parsed) {
        parameters.push_back(parameter.second);
    }
    BlockPtr body = blockStatement();
    return std::make_shared<FunctionExpr>(std::move(parameters), std::move(body));
}

ExprPtr Parser::primary() {
    if (match({T_BOOL_LIT})) {
        return std::make_shared<LiteralExpr>(makeBool(previous().bval));
    }
    if (match({T_INT_LIT})) {
        return std::make_shared<LiteralExpr>(makeInt(previous().ival));
    }
    if (match({T_FLOAT_LIT})) {
        return std::make_shared<LiteralExpr>(makeDouble(previous().dval));
    }
    if (match({T_STRING_LIT, T_CHAR_LIT})) {
        return std::make_shared<LiteralExpr>(makeString(previous().text));
    }
    if (match({T_IDENT})) {
        const Token& nameToken = previous();
        return std::make_shared<VarExpr>(nameToken.text, nameToken.dollar);
    }
    if (match({T_FUNCTION})) {
        return functionExpression();
    }
    if (match({T_LPAREN})) {
        ExprPtr expr = expression();
        consume(T_RPAREN, "Expected ')' after expression");
        return expr;
    }
    throw LangError(lineMessage(peek(), "Expected expression"));
}

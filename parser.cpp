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
    bool isAsync = match({T_ASYNC});

    if (match({T_FN})) {
        return functionDeclaration(T_VOID, isAsync);
    }
    if (isAsync && check(T_FUNCTION)) {
        ++current_;
        return functionDeclaration(T_VOID, true);
    }
    if (check(T_FUNCTION) && current_ + 1 < tokens_.size() &&
        tokens_[current_ + 1].type == T_IDENT) {
        ++current_;
        return functionDeclaration(T_VOID, isAsync);
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
    if (check(T_CLASS)) {
        return classDeclaration();
    }
    if (match({T_NAMESPACE})) {
        return namespaceDeclaration();
    }
    if (match({T_USING})) {
        return usingNamespaceStatement();
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

StmtPtr Parser::namespaceDeclaration() {
    std::string name = consume(T_IDENT, "Expected namespace name").text;
    while (match({T_DOUBLE_COLON})) {
        name += "::" + consume(T_IDENT, "Expected namespace name").text;
    }

    consume(T_LBRACE, "Expected '{' after namespace name");
    auto node = std::make_shared<NamespaceStmt>();
    node->name = name;
    while (!check(T_RBRACE) && !atEnd()) {
        if (check(T_ERROR)) {
            throw LangError(lineMessage(peek(), "Unexpected character: " + peek().text));
        }
        node->statements.push_back(declaration());
    }
    consume(T_RBRACE, "Expected '}' after namespace body");
    return node;
}

StmtPtr Parser::usingNamespaceStatement() {
    consume(T_NAMESPACE, "Expected 'namespace' after using");
    std::string name = consume(T_IDENT, "Expected namespace name").text;
    while (match({T_DOUBLE_COLON})) {
        name += "::" + consume(T_IDENT, "Expected namespace name").text;
    }
    consumeSemicolon();

    auto node = std::make_shared<UsingNamespaceStmt>();
    node->name = name;
    return node;
}

StmtPtr Parser::classDeclaration() {
    ++current_;
    const Token& nameToken = consume(T_IDENT, "Expected class name");

    std::string baseName;
    if (match({T_EXTENDS, T_EXTEND})) {
        baseName = consume(T_IDENT, "Expected base class name").text;
    } else if (match({T_COLON})) {
        if (check(T_PUBLIC) || check(T_PRIVATE) || check(T_PROTECTED)) {
            ++current_;
        }
        baseName = consume(T_IDENT, "Expected base class name").text;
    }

    consume(T_LBRACE, "Expected '{' after class name");

    auto node = std::make_shared<ClassDeclStmt>();
    node->name = nameToken.text;
    node->baseName = baseName;

    while (!check(T_RBRACE) && !atEnd()) {
        std::string visibility = "public";
        bool isStatic = false;
        while (check(T_PUBLIC) || check(T_PRIVATE) || check(T_PROTECTED) || check(T_STATIC)) {
            if (check(T_PRIVATE)) {
                visibility = "private";
            } else if (check(T_PROTECTED)) {
                visibility = "protected";
            } else if (check(T_STATIC)) {
                isStatic = true;
            }
            ++current_;
        }

        if (match({T_CONST})) {
            const Token& constantName = consume(T_IDENT, "Expected constant name");
            consume(T_ASSIGN, "Expected '=' after constant name");
            ExprPtr initializer = expression();
            consumeSemicolon();
            ClassConstantNode constant;
            constant.name = constantName.text;
            constant.initializer = std::move(initializer);
            node->constants.push_back(std::move(constant));
            continue;
        }

        if (check(T_FUNCTION) || check(T_FN)) {
            ++current_;
            const Token& methodName = consume(T_IDENT, "Expected method name");
            auto parsed = parameterList();
            ClassMethodNode method;
            method.name = methodName.text;
            for (const auto& parameter : parsed) {
                method.parameters.push_back(parameter.second);
            }
            method.body = blockStatement();
            method.visibility = visibility;
            method.isStatic = isStatic;
            node->methods.push_back(std::move(method));
            continue;
        }

        TokType declaredType = T_VOID;
        if (isTypeToken(peek().type)) {
            declaredType = peek().type;
            ++current_;
        }
        const Token& propertyName = consume(T_IDENT, "Expected property name");
        if (!propertyName.dollar) {
            throw LangError(lineMessage(propertyName, "Property must use a dollar sigil"));
        }
        ClassPropertyNode property;
        property.name = propertyName.text.substr(1);
        property.declaredType = declaredType;
        property.visibility = visibility;
        property.isStatic = isStatic;
        if (match({T_ASSIGN})) {
            property.initializer = expression();
        }
        consumeSemicolon();
        node->properties.push_back(std::move(property));
    }

    consume(T_RBRACE, "Expected '}' after class body");
    return node;
}

std::vector<std::pair<TokType, std::string>> Parser::parameterList() {
    consume(T_LPAREN, "Expected '(' after function name");
    std::vector<std::pair<TokType, std::string>> parameters;
    if (!check(T_RPAREN)) {
        do {
            if (check(T_IDENT)) {
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

StmtPtr Parser::functionDeclaration(TokType returnType, bool isAsync) {
    (void)returnType;
    const Token& nameToken = consume(T_IDENT, "Expected function name");
    std::vector<std::pair<TokType, std::string>> parameters = parameterList();

    BlockPtr body = blockStatement();
    auto function = std::make_shared<FunctionDeclStmt>();
    function->name = nameToken.text;
    function->parameters = std::move(parameters);
    function->body = std::move(body);
    function->isAsync = isAsync;
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

bool declaresNames(const StmtPtr& statement) {
    return std::dynamic_pointer_cast<VarDeclStmt>(statement) ||
           std::dynamic_pointer_cast<FunctionDeclStmt>(statement) ||
           std::dynamic_pointer_cast<ClassDeclStmt>(statement) ||
           std::dynamic_pointer_cast<NamespaceStmt>(statement);
}

BlockPtr Parser::blockStatement() {
    auto block = std::make_shared<BlockStmt>();
    consume(T_LBRACE, "Expected '{'");
    while (!check(T_RBRACE) && !atEnd()) {
        if (check(T_ERROR)) {
            throw LangError(lineMessage(peek(), "Unexpected character: " + peek().text));
        }
        StmtPtr statement = declaration();
        if (declaresNames(statement)) {
            block->needsScope = true;
        }
        block->statements.push_back(std::move(statement));
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
        return std::make_shared<AssignExpr>(left, std::move(value));
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
    if (match({T_AWAIT})) {
        return std::make_shared<AwaitExpr>(unary());
    }
    return call();
}

ExprPtr Parser::call() {
    ExprPtr expr = primary();

    while (true) {
        if (match({T_LPAREN})) {
            std::vector<ExprPtr> args;
            if (!check(T_RPAREN)) {
                do {
                    args.push_back(expression());
                } while (match({T_COMMA}));
            }
            consume(T_RPAREN, "Expected ')' after arguments");
            expr = std::make_shared<CallExpr>(expr, std::move(args));
            continue;
        }

        if (match({T_ARROW})) {
            const Token& member = consume(T_IDENT, "Expected property or method name");
            if (check(T_LPAREN)) {
                ++current_;
                std::vector<ExprPtr> args;
                if (!check(T_RPAREN)) {
                    do {
                        args.push_back(expression());
                    } while (match({T_COMMA}));
                }
                consume(T_RPAREN, "Expected ')' after arguments");
                expr = std::make_shared<MemberCallExpr>(expr, member.text, std::move(args));
            } else {
                expr = std::make_shared<MemberExpr>(expr, member.text);
            }
            continue;
        }

        if (match({T_DOUBLE_COLON})) {
            const Token& member = consume(T_IDENT, "Expected static member name");
            if (check(T_LPAREN)) {
                ++current_;
                std::vector<ExprPtr> args;
                if (!check(T_RPAREN)) {
                    do {
                        args.push_back(expression());
                    } while (match({T_COMMA}));
                }
                consume(T_RPAREN, "Expected ')' after arguments");
                expr = std::make_shared<StaticCallExpr>(expr, member.text, std::move(args));
            } else {
                expr = std::make_shared<StaticMemberExpr>(expr, member.text);
            }
            continue;
        }

        break;
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
    if (match({T_NULL})) {
        return std::make_shared<LiteralExpr>(makeNull());
    }
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
    if (match({T_ASYNC})) {
        consume(T_FUNCTION, "Expected 'function' after async");
        ExprPtr expr = functionExpression();
        auto functionExpr = std::dynamic_pointer_cast<FunctionExpr>(expr);
        if (functionExpr) {
            functionExpr->isAsync = true;
        }
        return expr;
    }
    if (match({T_THIS})) {
        return std::make_shared<ThisExpr>();
    }
    if (match({T_PARENT})) {
        return std::make_shared<ParentExpr>();
    }
    if (match({T_NEW})) {
        const Token& className = consume(T_IDENT, "Expected class name after new");
        std::vector<ExprPtr> args;
        if (match({T_LPAREN})) {
            if (!check(T_RPAREN)) {
                do {
                    args.push_back(expression());
                } while (match({T_COMMA}));
            }
            consume(T_RPAREN, "Expected ')' after constructor arguments");
        }
        return std::make_shared<NewExpr>(className.text, std::move(args));
    }
    if (match({T_LPAREN})) {
        ExprPtr expr = expression();
        consume(T_RPAREN, "Expected ')' after expression");
        return expr;
    }
    throw LangError(lineMessage(peek(), "Expected expression"));
}

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "token.hpp"

struct Interpreter;

enum class Flow { Normal, Return, Break, Continue };

struct ExecResult {
    Flow flow = Flow::Normal;
    Value value;
};

struct Expr {
    virtual ~Expr() = default;
    virtual Value eval(Interpreter& interpreter) = 0;
};

struct Stmt {
    virtual ~Stmt() = default;
    virtual ExecResult exec(Interpreter& interpreter) = 0;
};

struct BlockStmt;

using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;
using BlockPtr = std::shared_ptr<BlockStmt>;

struct LiteralExpr : Expr {
    Value value;

    explicit LiteralExpr(Value v) : value(std::move(v)) {}

    Value eval(Interpreter&) override;
};

struct VarExpr : Expr {
    std::string name;
    bool fromDollar = false;

    VarExpr(std::string n, bool dollar = false) : name(std::move(n)), fromDollar(dollar) {}

    Value eval(Interpreter& interpreter) override;
};

struct AssignExpr : Expr {
    std::string name;
    ExprPtr value;
    bool implicit = false;

    AssignExpr(std::string n, ExprPtr v, bool isImplicit = false)
        : name(std::move(n)), value(std::move(v)), implicit(isImplicit) {}

    Value eval(Interpreter& interpreter) override;
};

struct UnaryExpr : Expr {
    TokType op;
    ExprPtr operand;

    UnaryExpr(TokType o, ExprPtr e) : op(o), operand(std::move(e)) {}

    Value eval(Interpreter& interpreter) override;
};

struct BinaryExpr : Expr {
    TokType op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(TokType o, ExprPtr l, ExprPtr r)
        : op(o), left(std::move(l)), right(std::move(r)) {}

    Value eval(Interpreter& interpreter) override;
};

struct LogicalExpr : Expr {
    TokType op;
    ExprPtr left;
    ExprPtr right;

    LogicalExpr(TokType o, ExprPtr l, ExprPtr r)
        : op(o), left(std::move(l)), right(std::move(r)) {}

    Value eval(Interpreter& interpreter) override;
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    CallExpr(ExprPtr target, std::vector<ExprPtr> args)
        : callee(std::move(target)), arguments(std::move(args)) {}

    Value eval(Interpreter& interpreter) override;
};

struct FunctionExpr : Expr {
    std::vector<std::string> parameters;
    BlockPtr body;

    FunctionExpr(std::vector<std::string> params, BlockPtr b)
        : parameters(std::move(params)), body(std::move(b)) {}

    Value eval(Interpreter& interpreter) override;
};

struct EmptyStmt : Stmt {
    ExecResult exec(Interpreter&) override;
};

struct ExprStmt : Stmt {
    ExprPtr expression;

    explicit ExprStmt(ExprPtr e) : expression(std::move(e)) {}

    ExecResult exec(Interpreter& interpreter) override;
};

struct VarDeclStmt : Stmt {
    TokType declaredType = T_VOID;
    std::string name;
    ExprPtr initializer;
    bool isConst = false;
    bool isVar = false;

    ExecResult exec(Interpreter& interpreter) override;
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;

    ExecResult exec(Interpreter& interpreter) override;
};

struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;

    ExecResult exec(Interpreter& interpreter) override;
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;

    ExecResult exec(Interpreter& interpreter) override;
};

struct ForStmt : Stmt {
    StmtPtr initializer;
    ExprPtr condition;
    ExprPtr increment;
    StmtPtr body;

    ExecResult exec(Interpreter& interpreter) override;
};

struct ReturnStmt : Stmt {
    ExprPtr value;

    explicit ReturnStmt(ExprPtr e) : value(std::move(e)) {}

    ExecResult exec(Interpreter& interpreter) override;
};

struct BreakStmt : Stmt {
    ExecResult exec(Interpreter&) override;
};

struct ContinueStmt : Stmt {
    ExecResult exec(Interpreter&) override;
};

struct FunctionDeclStmt : Stmt {
    std::string name;
    std::vector<std::pair<TokType, std::string>> parameters;
    BlockPtr body;

    ExecResult exec(Interpreter& interpreter) override;
};

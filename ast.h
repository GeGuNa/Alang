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
    virtual void assign(Interpreter&, const Value&) {
        throw LangError("Invalid assignment target");
    }
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
    void assign(Interpreter& interpreter, const Value& value) override;
};

struct AssignExpr : Expr {
    ExprPtr target;
    ExprPtr value;

    AssignExpr(ExprPtr t, ExprPtr v) : target(std::move(t)), value(std::move(v)) {}

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
    bool isAsync = false;

    FunctionExpr(std::vector<std::string> params, BlockPtr b)
        : parameters(std::move(params)), body(std::move(b)) {}

    Value eval(Interpreter& interpreter) override;
};

struct AwaitExpr : Expr {
    ExprPtr value;

    explicit AwaitExpr(ExprPtr v) : value(std::move(v)) {}

    Value eval(Interpreter& interpreter) override;
};

struct ThisExpr : Expr {
    Value eval(Interpreter& interpreter) override;
};

struct ParentExpr : Expr {
    Value eval(Interpreter& interpreter) override;
};

struct NewExpr : Expr {
    std::string className;
    std::vector<ExprPtr> arguments;

    NewExpr(std::string name, std::vector<ExprPtr> args)
        : className(std::move(name)), arguments(std::move(args)) {}

    Value eval(Interpreter& interpreter) override;
};

struct MemberExpr : Expr {
    ExprPtr object;
    std::string name;

    MemberExpr(ExprPtr obj, std::string member)
        : object(std::move(obj)), name(std::move(member)) {}

    Value eval(Interpreter& interpreter) override;
    void assign(Interpreter& interpreter, const Value& value) override;
};

struct StaticMemberExpr : Expr {
    ExprPtr classExpr;
    std::string name;

    StaticMemberExpr(ExprPtr cls, std::string member)
        : classExpr(std::move(cls)), name(std::move(member)) {}

    Value eval(Interpreter& interpreter) override;
};

struct MemberCallExpr : Expr {
    ExprPtr object;
    std::string name;
    std::vector<ExprPtr> arguments;

    MemberCallExpr(ExprPtr obj, std::string member, std::vector<ExprPtr> args)
        : object(std::move(obj)), name(std::move(member)), arguments(std::move(args)) {}

    Value eval(Interpreter& interpreter) override;
};

struct StaticCallExpr : Expr {
    ExprPtr classExpr;
    std::string name;
    std::vector<ExprPtr> arguments;

    StaticCallExpr(ExprPtr cls, std::string member, std::vector<ExprPtr> args)
        : classExpr(std::move(cls)), name(std::move(member)), arguments(std::move(args)) {}

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
    bool needsScope = false;

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
    bool isAsync = false;

    ExecResult exec(Interpreter& interpreter) override;
};

struct ClassPropertyNode {
    std::string name;
    TokType declaredType = T_VOID;
    ExprPtr initializer;
    std::string visibility = "public";
    bool isStatic = false;
};

struct ClassConstantNode {
    std::string name;
    ExprPtr initializer;
};

struct ClassMethodNode {
    std::string name;
    std::vector<std::string> parameters;
    BlockPtr body;
    std::string visibility = "public";
    bool isStatic = false;
};

struct ClassDeclStmt : Stmt {
    std::string name;
    std::string baseName;
    std::vector<ClassPropertyNode> properties;
    std::vector<ClassConstantNode> constants;
    std::vector<ClassMethodNode> methods;

    ExecResult exec(Interpreter& interpreter) override;
};

struct NamespaceStmt : Stmt {
    std::string name;
    std::vector<StmtPtr> statements;

    ExecResult exec(Interpreter& interpreter) override;
};

struct UsingNamespaceStmt : Stmt {
    std::string name;

    ExecResult exec(Interpreter& interpreter) override;
};

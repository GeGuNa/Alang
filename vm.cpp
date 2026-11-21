#include "vm.h"

#include <algorithm>
#include <utility>

namespace {

struct SymbolTable {
    std::unordered_map<std::string, int> globals;
    int count = 0;

    int slot(const std::string& name) {
        auto it = globals.find(name);
        if (it != globals.end()) {
            return it->second;
        }
        int index = count++;
        globals[name] = index;
        return index;
    }
};

struct LoopInfo {
    std::vector<int> breaks;
    std::vector<int> continues;
};

class Compiler {
public:
    Compiler(FunctionChunk& chunk, SymbolTable& symbols, Compiler* parent, bool allowLocals)
        : chunk_(chunk), symbols_(symbols), parent_(parent), allowLocals_(allowLocals) {}

    void compileProgram(const std::vector<StmtPtr>& program) {
        for (const StmtPtr& statement : program) {
            compileStatement(statement);
        }
        emit(Op::HALT);
    }

    void compileFunction(const std::vector<std::string>& params, const BlockPtr& body) {
        scopes_.emplace_back();
        for (const std::string& param : params) {
            int slot = allocateLocal();
            scopes_.back()[param] = slot;
        }
        chunk_.paramNames = params;
        for (const StmtPtr& statement : body->statements) {
            compileStatement(statement);
        }
        emit(Op::RETURN_VOID);
        chunk_.localCount = localCount_;
        scopes_.clear();
    }

private:
    FunctionChunk& chunk_;
    SymbolTable& symbols_;
    Compiler* parent_;
    bool allowLocals_;
    std::vector<std::unordered_map<std::string, int>> scopes_;
    std::vector<LoopInfo> loops_;
    int localCount_ = 0;

    int constantIndex(const Value& value) {
        chunk_.constants.push_back(value);
        return static_cast<int>(chunk_.constants.size()) - 1;
    }

    int nameIndex(const std::string& name) {
        chunk_.names.push_back(name);
        return static_cast<int>(chunk_.names.size()) - 1;
    }

    int expressionIndex(const ExprPtr& expr) {
        chunk_.expressions.push_back(expr);
        return static_cast<int>(chunk_.expressions.size()) - 1;
    }

    int statementIndex(const StmtPtr& stmt) {
        chunk_.statements.push_back(stmt);
        return static_cast<int>(chunk_.statements.size()) - 1;
    }

    int emit(Op op, int a = 0, int b = 0) {
        Instruction instruction;
        instruction.op = op;
        instruction.a = a;
        instruction.b = b;
        chunk_.code.push_back(instruction);
        return static_cast<int>(chunk_.code.size()) - 1;
    }

    int allocateLocal() {
        return localCount_++;
    }

    bool findLocal(const std::string& name, int& slot) {
        if (!allowLocals_) {
            return false;
        }
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                slot = found->second;
                return true;
            }
        }
        return false;
    }

    bool findOuter(const std::string& name) {
        for (Compiler* current = parent_; current != nullptr; current = current->parent_) {
            int slot = 0;
            if (current->allowLocals_ && current->findLocal(name, slot)) {
                chunk_.legacy = true;
                for (Compiler* ancestor = parent_; ancestor != nullptr; ancestor = ancestor->parent_) {
                    ancestor->chunk_.legacy = true;
                }
                return true;
            }
        }
        return false;
    }

    bool findVariable(const std::string& name, int& slot, bool& isLocal) {
        if (findLocal(name, slot)) {
            isLocal = true;
            return true;
        }
        auto it = symbols_.globals.find(name);
        if (it != symbols_.globals.end()) {
            isLocal = false;
            slot = it->second;
            return true;
        }
        return false;
    }

    int declareSlot(const std::string& name, bool& isLocal) {
        if (!allowLocals_) {
            isLocal = false;
            return symbols_.slot(name);
        }
        int slot = allocateLocal();
        if (!scopes_.empty()) {
            scopes_.back()[name] = slot;
        }
        isLocal = true;
        return slot;
    }

    int ensureSlot(const std::string& name, bool& isLocal) {
        int slot = 0;
        if (findVariable(name, slot, isLocal)) {
            return slot;
        }
        if (allowLocals_ && findOuter(name)) {
            chunk_.legacy = true;
        }
        isLocal = false;
        return symbols_.slot(name);
    }

    int placeholder() {
        return emit(Op::JUMP, 0);
    }

    void patch(int index, int target) {
        chunk_.code[index].a = target;
    }

    int here() const {
        return static_cast<int>(chunk_.code.size());
    }

    void compileExpression(const ExprPtr& expr) {
        if (!expr) {
            emit(Op::CONST, constantIndex(makeVoid()));
            return;
        }

        if (auto awaitExpr = std::dynamic_pointer_cast<AwaitExpr>(expr)) {
            compileExpression(awaitExpr->value);
            emit(Op::AWAIT);
            return;
        }

        if (std::dynamic_pointer_cast<ArrayLiteralExpr>(expr) ||
            std::dynamic_pointer_cast<MapLiteralExpr>(expr) ||
            std::dynamic_pointer_cast<IndexExpr>(expr)) {
            emit(Op::EVAL_NODE, expressionIndex(expr));
            return;
        }

        if (auto literal = std::dynamic_pointer_cast<LiteralExpr>(expr)) {
            emit(Op::CONST, constantIndex(literal->value));
            return;
        }

        if (auto unary = std::dynamic_pointer_cast<UnaryExpr>(expr)) {
            compileExpression(unary->operand);
            emit(unary->op == T_NOT ? Op::NOT : Op::NEG);
            return;
        }

        if (auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
            compileExpression(binary->left);
            compileExpression(binary->right);
            Op op = Op::ADD;
            switch (binary->op) {
                case T_PLUS: op = Op::ADD; break;
                case T_MINUS: op = Op::SUB; break;
                case T_STAR: op = Op::MUL; break;
                case T_SLASH: op = Op::DIV; break;
                case T_PERCENT: op = Op::MOD; break;
                case T_DOT: op = Op::CONCAT; break;
                case T_EQ: op = Op::EQ; break;
                case T_NE: op = Op::NE; break;
                case T_GT: op = Op::GT; break;
                case T_GE: op = Op::GE; break;
                case T_LT: op = Op::LT; break;
                case T_LE: op = Op::LE; break;
                default: break;
            }
            emit(op);
            return;
        }

        if (auto logical = std::dynamic_pointer_cast<LogicalExpr>(expr)) {
            compileExpression(logical->left);
            if (logical->op == T_AND) {
                int skip = emit(Op::JUMP_IF_FALSE, 0);
                compileExpression(logical->right);
                emit(Op::BOOL);
                int end = placeholder();
                patch(skip, here());
                emit(Op::CONST, constantIndex(makeBool(false)));
                patch(end, here());
            } else {
                int skip = emit(Op::JUMP_IF_TRUE, 0);
                compileExpression(logical->right);
                emit(Op::BOOL);
                int end = placeholder();
                patch(skip, here());
                emit(Op::CONST, constantIndex(makeBool(true)));
                patch(end, here());
            }
            return;
        }

        if (auto variable = std::dynamic_pointer_cast<VarExpr>(expr)) {
            int slot = 0;
            bool isLocal = false;
            if (findVariable(variable->name, slot, isLocal)) {
                emit(Op::LOAD_SLOT, slot, isLocal ? 1 : 0);
            } else {
                if (allowLocals_) {
                    findOuter(variable->name);
                }
                emit(Op::LOAD_NAME, nameIndex(variable->name));
            }
            return;
        }

        if (auto assignment = std::dynamic_pointer_cast<AssignExpr>(expr)) {
            auto target = std::dynamic_pointer_cast<VarExpr>(assignment->target);
            if (target) {
                int existingSlot = 0;
                bool existingLocal = false;
                if (!target->fromDollar && !findVariable(target->name, existingSlot, existingLocal)) {
                    emit(Op::EVAL_NODE, expressionIndex(expr));
                    return;
                }
                bool isLocal = false;
                int slot = ensureSlot(target->name, isLocal);
                compileExpression(assignment->value);
                emit(Op::DUP);
                int storeIns = emit(Op::STORE_SLOT, slot, isLocal ? 1 : 0);
                chunk_.code[storeIns].c = nameIndex(target->name);
                return;
            }
            auto member = std::dynamic_pointer_cast<MemberExpr>(assignment->target);
            if (member) {
                compileExpression(member->object);
                compileExpression(assignment->value);
                emit(Op::STORE_MEMBER, nameIndex(member->name));
                return;
            }
            auto staticMember = std::dynamic_pointer_cast<StaticMemberExpr>(assignment->target);
            if (staticMember) {
                compileExpression(staticMember->classExpr);
                compileExpression(assignment->value);
                emit(Op::STORE_STATIC, nameIndex(staticMember->name));
                return;
            }
            emit(Op::EVAL_NODE, expressionIndex(expr));
            return;
        }

        if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
            auto callee = std::dynamic_pointer_cast<VarExpr>(call->callee);
            bool isNative = callee && (callee->name == "print" || callee->name == "echo" ||
                                       callee->name == "println");
            if (!isNative) {
                compileExpression(call->callee);
            }
            for (const ExprPtr& argument : call->arguments) {
                compileExpression(argument);
            }
            if (isNative) {
                emit(Op::CALL_NATIVE, nameIndex(callee->name), static_cast<int>(call->arguments.size()));
            } else {
                emit(Op::CALL, static_cast<int>(call->arguments.size()));
            }
            return;
        }

        if (auto member = std::dynamic_pointer_cast<MemberExpr>(expr)) {
            compileExpression(member->object);
            emit(Op::LOAD_MEMBER, nameIndex(member->name));
            return;
        }

        if (auto memberCall = std::dynamic_pointer_cast<MemberCallExpr>(expr)) {
            compileExpression(memberCall->object);
            for (const ExprPtr& argument : memberCall->arguments) {
                compileExpression(argument);
            }
            emit(Op::CALL_METHOD, nameIndex(memberCall->name), static_cast<int>(memberCall->arguments.size()));
            return;
        }

        if (auto staticMember = std::dynamic_pointer_cast<StaticMemberExpr>(expr)) {
            compileExpression(staticMember->classExpr);
            emit(Op::LOAD_STATIC, nameIndex(staticMember->name));
            return;
        }

        if (auto staticCall = std::dynamic_pointer_cast<StaticCallExpr>(expr)) {
            compileExpression(staticCall->classExpr);
            for (const ExprPtr& argument : staticCall->arguments) {
                compileExpression(argument);
            }
            emit(Op::CALL_STATIC, nameIndex(staticCall->name), static_cast<int>(staticCall->arguments.size()));
            return;
        }

        if (auto newExpr = std::dynamic_pointer_cast<NewExpr>(expr)) {
            for (const ExprPtr& argument : newExpr->arguments) {
                compileExpression(argument);
            }
            emit(Op::NEW, nameIndex(newExpr->className), static_cast<int>(newExpr->arguments.size()));
            return;
        }

        if (std::dynamic_pointer_cast<ThisExpr>(expr)) {
            emit(Op::THIS);
            return;
        }

        if (std::dynamic_pointer_cast<ParentExpr>(expr)) {
            emit(Op::PARENT);
            return;
        }

        if (std::dynamic_pointer_cast<FunctionExpr>(expr)) {
            compileFunctionExpression(std::dynamic_pointer_cast<FunctionExpr>(expr));
            return;
        }

        emit(Op::EVAL_NODE, expressionIndex(expr));
    }

    void compileFunctionExpression(const std::shared_ptr<FunctionExpr>& expr) {
        auto child = std::make_shared<FunctionChunk>();
        int index = static_cast<int>(chunk_.children.size());
        chunk_.children.push_back(child);
        child->isAsync = expr->isAsync;
        child->astBody = expr->body;
        Compiler childCompiler(*child, symbols_, this, true);
        childCompiler.compileFunction(expr->parameters, expr->body);
        for (const auto& grandchild : child->children) {
            if (grandchild->legacy) {
                child->legacy = true;
            }
        }
        if (child->legacy) {
            emit(Op::EVAL_NODE, expressionIndex(expr));
        } else {
            emit(Op::MAKE_FUNCTION, index);
        }
    }

    void compileStatement(const StmtPtr& statement) {
        if (!statement) {
            return;
        }

        if (std::dynamic_pointer_cast<EmptyStmt>(statement)) {
            return;
        }

        if (auto expressionStatement = std::dynamic_pointer_cast<ExprStmt>(statement)) {
            compileExpression(expressionStatement->expression);
            emit(Op::POP);
            return;
        }

        if (auto declaration = std::dynamic_pointer_cast<VarDeclStmt>(statement)) {
            bool isLocal = false;
            int slot = declareSlot(declaration->name, isLocal);
            if (declaration->initializer) {
                compileExpression(declaration->initializer);
            } else {
                Value value = makeVoid();
                switch (declaration->declaredType) {
                    case T_INT_TYPE: value = makeInt(0); break;
                    case T_FLOAT_TYPE:
                    case T_DOUBLE_TYPE: value = makeDouble(0.0); break;
                    case T_BOOL: value = makeBool(false); break;
                    case T_STRING_TYPE:
                    case T_STR_TYPE:
                    case T_CHAR_TYPE: value = makeString(""); break;
                    default: break;
                }
                emit(Op::CONST, constantIndex(value));
            }
            int flags = 0;
            if (declaration->isConst) flags |= 1;
            if (declaration->isVar) flags |= 2;
            int instruction = emit(Op::DEFINE_VAR, slot, flags | (isLocal ? 4 : 0));
            chunk_.code[instruction].c = nameIndex(declaration->name);
            return;
        }

        if (std::dynamic_pointer_cast<ForEachStmt>(statement) ||
            std::dynamic_pointer_cast<SwitchStmt>(statement) ||
            std::dynamic_pointer_cast<TryStmt>(statement) ||
            std::dynamic_pointer_cast<ThrowStmt>(statement)) {
            emit(Op::EXEC_NODE, statementIndex(statement));
            return;
        }

        if (auto block = std::dynamic_pointer_cast<BlockStmt>(statement)) {
            if (block->needsScope && !allowLocals_) {
                emit(Op::EXEC_NODE, statementIndex(statement));
                return;
            }
            if (block->needsScope && allowLocals_) {
                scopes_.emplace_back();
                for (const StmtPtr& inner : block->statements) {
                    compileStatement(inner);
                }
                scopes_.pop_back();
            } else {
                for (const StmtPtr& inner : block->statements) {
                    compileStatement(inner);
                }
            }
            return;
        }

        if (auto ifStatement = std::dynamic_pointer_cast<IfStmt>(statement)) {
            compileExpression(ifStatement->condition);
            int falseJump = emit(Op::JUMP_IF_FALSE, 0);
            compileStatement(ifStatement->thenBranch);
            int endJump = placeholder();
            patch(falseJump, here());
            if (ifStatement->elseBranch) {
                compileStatement(ifStatement->elseBranch);
            }
            patch(endJump, here());
            return;
        }

        if (auto whileStatement = std::dynamic_pointer_cast<WhileStmt>(statement)) {
            int start = here();
            if (whileStatement->condition) {
                compileExpression(whileStatement->condition);
            } else {
                emit(Op::CONST, constantIndex(makeBool(true)));
            }
            int exitJump = emit(Op::JUMP_IF_FALSE, 0);
            LoopInfo loop;
            loops_.push_back(loop);
            size_t loopIndex = loops_.size() - 1;
            compileStatement(whileStatement->body);
            for (int jump : loops_[loopIndex].continues) patch(jump, start);
            emit(Op::JUMP, start);
            patch(exitJump, here());
            for (int jump : loops_[loopIndex].breaks) patch(jump, here());
            loops_.pop_back();
            return;
        }

        if (auto forStatement = std::dynamic_pointer_cast<ForStmt>(statement)) {
            compileStatement(forStatement->initializer);
            int start = here();
            if (forStatement->condition) {
                compileExpression(forStatement->condition);
            } else {
                emit(Op::CONST, constantIndex(makeBool(true)));
            }
            int exitJump = emit(Op::JUMP_IF_FALSE, 0);
            LoopInfo loop;
            loops_.push_back(loop);
            size_t loopIndex = loops_.size() - 1;
            compileStatement(forStatement->body);
            int continueTarget = here();
            for (int jump : loops_[loopIndex].continues) patch(jump, continueTarget);
            if (forStatement->increment) {
                compileExpression(forStatement->increment);
                emit(Op::POP);
            }
            emit(Op::JUMP, start);
            patch(exitJump, here());
            for (int jump : loops_[loopIndex].breaks) patch(jump, here());
            loops_.pop_back();
            return;
        }

        if (auto returnStatement = std::dynamic_pointer_cast<ReturnStmt>(statement)) {
            if (returnStatement->value) {
                compileExpression(returnStatement->value);
            } else {
                emit(Op::CONST, constantIndex(makeVoid()));
            }
            emit(Op::RETURN);
            return;
        }

        if (std::dynamic_pointer_cast<BreakStmt>(statement)) {
            int jump = placeholder();
            loops_.back().breaks.push_back(jump);
            return;
        }

        if (std::dynamic_pointer_cast<ContinueStmt>(statement)) {
            int jump = placeholder();
            loops_.back().continues.push_back(jump);
            return;
        }

        if (auto function = std::dynamic_pointer_cast<FunctionDeclStmt>(statement)) {
            auto child = std::make_shared<FunctionChunk>();
            int index = static_cast<int>(chunk_.children.size());
            chunk_.children.push_back(child);
            std::vector<std::string> params;
            for (const auto& parameter : function->parameters) {
                params.push_back(parameter.second);
            }
            child->isAsync = function->isAsync;
            child->astBody = function->body;
            Compiler childCompiler(*child, symbols_, this, true);
            childCompiler.compileFunction(params, function->body);
            for (const auto& grandchild : child->children) {
                if (grandchild->legacy) {
                    child->legacy = true;
                }
            }
            if (child->legacy) {
                emit(Op::EXEC_NODE, statementIndex(statement));
                return;
            }
            emit(Op::MAKE_FUNCTION, index);
            bool isLocal = false;
            int slot = ensureSlot(function->name, isLocal);
            emit(Op::DUP);
            emit(Op::STORE_SLOT, slot, isLocal ? 1 : 0);
            emit(Op::DEFINE_FUNCTION, nameIndex(function->name));
            return;
        }

        emit(Op::EXEC_NODE, statementIndex(statement));
    }
};

}

std::shared_ptr<FunctionChunk> compileProgram(const std::vector<StmtPtr>& program) {
    auto chunk = std::make_shared<FunctionChunk>();
    SymbolTable symbols;
    Compiler compiler(*chunk, symbols, nullptr, false);
    compiler.compileProgram(program);
    chunk->localCount = symbols.count;
    chunk->globalSlots = symbols.globals;
    return chunk;
}

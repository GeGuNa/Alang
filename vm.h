#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"
#include "token.hpp"

enum class Op : std::uint8_t {
    CONST,
    LOAD_SLOT,
    STORE_SLOT,
    LOAD_NAME,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    CONCAT,
    EQ,
    NE,
    GT,
    GE,
    LT,
    LE,
    AND,
    OR,
    NOT,
    NEG,
    JUMP,
    JUMP_IF_FALSE,
    JUMP_IF_TRUE,
    POP,
    DUP,
    CALL,
    CALL_METHOD,
    CALL_STATIC,
    LOAD_MEMBER,
    STORE_MEMBER,
    LOAD_STATIC,
    NEW,
    THIS,
    PARENT,
    MAKE_FUNCTION,
    DEFINE_FUNCTION,
    DEFINE_VAR,
    BOOL,
    EXEC_NODE,
    EVAL_NODE,
    RETURN,
    RETURN_VOID,
    HALT,
    CALL_NATIVE,
    AWAIT,
    STORE_STATIC
};

struct Instruction {
    Op op = Op::HALT;
    int a = 0;
    int b = 0;
    int c = 0;
};

struct FunctionChunk {
    std::vector<Instruction> code;
    std::vector<Value> constants;
    std::vector<std::string> names;
    std::vector<std::shared_ptr<FunctionChunk>> children;
    std::vector<StmtPtr> statements;
    std::vector<ExprPtr> expressions;
    int localCount = 0;
    bool legacy = false;
    std::vector<std::string> paramNames;
    std::unordered_map<std::string, int> globalSlots;
    std::shared_ptr<BlockStmt> astBody;
    bool isAsync = false;
    std::shared_ptr<BlockStmt> legacyBody;
};

std::shared_ptr<FunctionChunk> compileProgram(const std::vector<StmtPtr>& program);

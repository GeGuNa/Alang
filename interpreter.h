#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.h"
#include "token.hpp"

struct Environment {
    std::unordered_map<std::string, Value> values;
    std::unordered_map<std::string, ValueType> inferredTypes;
    std::unordered_set<std::string> constants;
    std::shared_ptr<Environment> parent;
    std::weak_ptr<Environment> globalParent;
    bool isGlobal = false;

    explicit Environment(std::shared_ptr<Environment> p = nullptr) {
        if (p && p->isGlobal) {
            globalParent = p;
        } else {
            parent = std::move(p);
        }
    }

    void define(const std::string& name, Value value);
    void defineConst(const std::string& name, Value value);
    void defineInferred(const std::string& name, const Value& value);
    bool get(const std::string& name, Value& out) const;
    bool assign(const std::string& name, const Value& value);
};

struct FunctionValue {
    std::vector<std::string> parameters;
    BlockPtr body;
    std::string nativeName;
    std::shared_ptr<Environment> closure;
};

using NativeFn = std::function<Value(Interpreter&, const std::vector<Value>&)>;

struct Interpreter {
    std::shared_ptr<Environment> globals = std::make_shared<Environment>();
    std::shared_ptr<Environment> environment = globals;
    std::unordered_map<std::string, FunctionValue> functions;
    std::unordered_map<std::string, NativeFn> natives;

    Interpreter();

    Value callFunctionValue(const FunctionValue& function, const std::vector<Value>& args);
    ExecResult executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> env);
};

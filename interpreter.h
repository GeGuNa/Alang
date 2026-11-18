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
    std::string className;
    std::string visibility = "public";
    bool isStatic = false;
};

struct ClassInfo {
    std::string name;
    std::shared_ptr<ClassInfo> parent;
    std::unordered_map<std::string, Value> defaultProperties;
    std::unordered_map<std::string, std::string> propertyVisibility;
    std::unordered_map<std::string, Value> constants;
    std::unordered_map<std::string, FunctionValue> methods;
};

struct ObjectInstance {
    std::shared_ptr<ClassInfo> classInfo;
    std::unordered_map<std::string, Value> properties;
    bool constructed = false;
    bool destructed = false;
};

struct NamespaceInfo {
    std::string name;
    std::weak_ptr<NamespaceInfo> parent;
    std::unordered_map<std::string, std::shared_ptr<NamespaceInfo>> namespaces;
    std::unordered_map<std::string, std::shared_ptr<ClassInfo>> classes;
    std::unordered_map<std::string, FunctionValue> functions;
    std::unordered_map<std::string, Value> constants;
};

using NativeFn = std::function<Value(Interpreter&, const std::vector<Value>&)>;

struct Interpreter : std::enable_shared_from_this<Interpreter> {
    std::shared_ptr<Environment> globals = std::make_shared<Environment>();
    std::shared_ptr<Environment> environment = globals;
    std::unordered_map<std::string, FunctionValue> functions;
    std::unordered_map<std::string, NativeFn> natives;
    std::unordered_map<std::string, std::shared_ptr<ClassInfo>> classes;
    std::unordered_map<std::string, std::shared_ptr<NamespaceInfo>> namespaces;
    std::vector<std::shared_ptr<NamespaceInfo>> usingNamespaces;
    std::shared_ptr<NamespaceInfo> currentNamespace;
    std::vector<Value> thisStack;
    std::vector<std::string> thisClassStack;
    bool shuttingDown = false;

    Interpreter();
    ~Interpreter();

    void shutdown();

    Value callFunctionValue(const FunctionValue& function, const std::vector<Value>& args);
    Value callMethod(const Value& object, const std::string& name, const std::vector<Value>& args);
    Value callMethodFrom(const std::shared_ptr<ClassInfo>& info, const Value& object,
                         const std::string& name, const std::vector<Value>& args);
    Value callStaticMethod(const std::shared_ptr<ClassInfo>& info, const std::string& name,
                           const std::vector<Value>& args);
    Value currentThis() const;
    std::string currentClass() const;
    void checkVisibility(const std::string& ownerClass, const std::string& visibility) const;
    bool isSubclassOf(const std::string& child, const std::string& parent) const;
    ExecResult executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> env);
};

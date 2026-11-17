#include "interpreter.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>
#include <utility>

bool truthy(const Value& value) {
    switch (value.type) {
        case ValueType::Void: return false;
        case ValueType::Int: return value.ival != 0;
        case ValueType::Double: return value.dval != 0.0;
        case ValueType::Bool: return value.bval;
        case ValueType::String: return !value.sval.empty();
        case ValueType::Function: return static_cast<bool>(value.fval);
    }
    return false;
}

double asNumber(const Value& value) {
    switch (value.type) {
        case ValueType::Int: return static_cast<double>(value.ival);
        case ValueType::Double: return value.dval;
        case ValueType::Bool: return value.bval ? 1.0 : 0.0;
        case ValueType::String:
            try {
                return std::stod(value.sval);
            } catch (const std::exception&) {
                return 0.0;
            }
        case ValueType::Void: return 0.0;
        case ValueType::Function: return 0.0;
    }
    return 0.0;
}

std::string valueToString(const Value& value) {
    switch (value.type) {
        case ValueType::Void: return "void";
        case ValueType::Int: return std::to_string(value.ival);
        case ValueType::Double: {
            std::ostringstream stream;
            stream << value.dval;
            return stream.str();
        }
        case ValueType::Bool: return value.bval ? "true" : "false";
        case ValueType::String: return value.sval;
        case ValueType::Function: return "<function>";
    }
    return "";
}

bool valuesEqual(const Value& left, const Value& right) {
    if (left.type == ValueType::Function || right.type == ValueType::Function) {
        return left.fval == right.fval;
    }
    if (left.type == ValueType::String || right.type == ValueType::String) {
        return valueToString(left) == valueToString(right);
    }
    if (left.type == ValueType::Bool || right.type == ValueType::Bool) {
        return truthy(left) == truthy(right);
    }
    if (left.type == ValueType::Void && right.type == ValueType::Void) {
        return true;
    }
    return asNumber(left) == asNumber(right);
}

int compareValues(const Value& left, const Value& right) {
    if (left.type == ValueType::String && right.type == ValueType::String) {
        if (left.sval < right.sval) return -1;
        if (left.sval > right.sval) return 1;
        return 0;
    }
    double a = asNumber(left);
    double b = asNumber(right);
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

Value defaultValue(TokType type) {
    switch (type) {
        case T_INT_TYPE: return makeInt(0);
        case T_FLOAT_TYPE:
        case T_DOUBLE_TYPE: return makeDouble(0.0);
        case T_BOOL: return makeBool(false);
        case T_STRING_TYPE:
        case T_STR_TYPE:
        case T_CHAR_TYPE: return makeString("");
        default: return makeVoid();
    }
}

void detachSelfCapture(Value& value, Environment* scope) {
    if (value.type == ValueType::Function && value.fval && value.fval->closure.get() == scope) {
        value.fval->closure.reset();
    }
}

void Environment::define(const std::string& name, Value value) {
    if (constants.count(name) != 0) {
        throw LangError("Cannot redeclare constant: " + name);
    }
    constants.erase(name);
    inferredTypes.erase(name);
    detachSelfCapture(value, this);
    values[name] = std::move(value);
}

void Environment::defineConst(const std::string& name, Value value) {
    if (constants.count(name) != 0) {
        throw LangError("Cannot redeclare constant: " + name);
    }
    constants.insert(name);
    inferredTypes.erase(name);
    detachSelfCapture(value, this);
    values[name] = std::move(value);
}

void Environment::defineInferred(const std::string& name, const Value& value) {
    if (constants.count(name) != 0) {
        throw LangError("Cannot redeclare constant: " + name);
    }
    constants.erase(name);
    Value stored = value;
    detachSelfCapture(stored, this);
    values[name] = std::move(stored);
    inferredTypes[name] = value.type;
}

bool Environment::get(const std::string& name, Value& out) const {
    const Environment* current = this;
    std::shared_ptr<Environment> held;
    while (current != nullptr) {
        auto it = current->values.find(name);
        if (it != current->values.end()) {
            out = it->second;
            return true;
        }
        if (current->parent) {
            current = current->parent.get();
            continue;
        }
        held = current->globalParent.lock();
        if (!held) {
            break;
        }
        current = held.get();
    }
    return false;
}

bool Environment::assign(const std::string& name, const Value& value) {
    Environment* current = this;
    std::shared_ptr<Environment> held;
    while (current != nullptr) {
        auto it = current->values.find(name);
        if (it != current->values.end()) {
            if (current->constants.count(name) != 0) {
                throw LangError("Cannot assign to constant: " + name);
            }
            auto typeIt = current->inferredTypes.find(name);
            if (typeIt != current->inferredTypes.end() && typeIt->second != value.type) {
                throw LangError("Type mismatch for variable: " + name);
            }
            it->second = value;
            return true;
        }
        if (current->parent) {
            current = current->parent.get();
            continue;
        }
        held = current->globalParent.lock();
        if (!held) {
            break;
        }
        current = held.get();
    }
    return false;
}

struct EnvGuard {
    Interpreter& interpreter;
    std::shared_ptr<Environment> saved;

    explicit EnvGuard(Interpreter& i) : interpreter(i), saved(i.environment) {}

    ~EnvGuard() {
        interpreter.environment = saved;
    }
};

Value makeFunctionValue(const FunctionValue& function) {
    Value value;
    value.type = ValueType::Function;
    value.fval = std::make_shared<FunctionValue>(function);
    return value;
}

Value LiteralExpr::eval(Interpreter&) {
    return value;
}

Value VarExpr::eval(Interpreter& interpreter) {
    Value value;
    if (interpreter.environment->get(name, value)) {
        return value;
    }

    auto nativeIt = interpreter.natives.find(name);
    if (nativeIt != interpreter.natives.end()) {
        FunctionValue function;
        function.nativeName = name;
        return makeFunctionValue(function);
    }

    auto functionIt = interpreter.functions.find(name);
    if (functionIt != interpreter.functions.end()) {
        return makeFunctionValue(functionIt->second);
    }

    throw LangError("Undefined variable: " + name);
}

Value AssignExpr::eval(Interpreter& interpreter) {
    Value value = this->value->eval(interpreter);
    if (interpreter.environment->assign(name, value)) {
        return value;
    }
    if (implicit) {
        interpreter.environment->defineInferred(name, value);
        return value;
    }
    throw LangError("Undefined variable: " + name);
}

Value UnaryExpr::eval(Interpreter& interpreter) {
    Value operandValue = operand->eval(interpreter);

    if (op == T_NOT) {
        return makeBool(!truthy(operandValue));
    }
    if (op == T_MINUS) {
        if (operandValue.type == ValueType::Int) {
            return makeInt(-operandValue.ival);
        }
        return makeDouble(-asNumber(operandValue));
    }
    throw LangError("Unsupported unary operator");
}

Value BinaryExpr::eval(Interpreter& interpreter) {
    Value leftValue = left->eval(interpreter);
    Value rightValue = right->eval(interpreter);

    switch (op) {
        case T_PLUS:
            if (leftValue.type == ValueType::String || rightValue.type == ValueType::String) {
                return makeString(valueToString(leftValue) + valueToString(rightValue));
            }
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                return makeInt(leftValue.ival + rightValue.ival);
            }
            return makeDouble(asNumber(leftValue) + asNumber(rightValue));
        case T_MINUS:
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                return makeInt(leftValue.ival - rightValue.ival);
            }
            return makeDouble(asNumber(leftValue) - asNumber(rightValue));
        case T_STAR:
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                return makeInt(leftValue.ival * rightValue.ival);
            }
            return makeDouble(asNumber(leftValue) * asNumber(rightValue));
        case T_SLASH:
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                if (rightValue.ival == 0) {
                    throw LangError("Division by zero");
                }
                return makeInt(leftValue.ival / rightValue.ival);
            }
            if (asNumber(rightValue) == 0.0) {
                throw LangError("Division by zero");
            }
            return makeDouble(asNumber(leftValue) / asNumber(rightValue));
        case T_PERCENT:
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                if (rightValue.ival == 0) {
                    throw LangError("Division by zero");
                }
                return makeInt(leftValue.ival % rightValue.ival);
            }
            if (asNumber(rightValue) == 0.0) {
                throw LangError("Division by zero");
            }
            return makeDouble(std::fmod(asNumber(leftValue), asNumber(rightValue)));
        case T_GT: return makeBool(compareValues(leftValue, rightValue) > 0);
        case T_GE: return makeBool(compareValues(leftValue, rightValue) >= 0);
        case T_LT: return makeBool(compareValues(leftValue, rightValue) < 0);
        case T_LE: return makeBool(compareValues(leftValue, rightValue) <= 0);
        case T_EQ: return makeBool(valuesEqual(leftValue, rightValue));
        case T_NE: return makeBool(!valuesEqual(leftValue, rightValue));
        case T_DOT: return makeString(valueToString(leftValue) + valueToString(rightValue));
        default: throw LangError("Unsupported binary operator");
    }
}

Value LogicalExpr::eval(Interpreter& interpreter) {
    Value leftValue = left->eval(interpreter);

    if (op == T_AND) {
        if (!truthy(leftValue)) {
            return makeBool(false);
        }
        return makeBool(truthy(right->eval(interpreter)));
    }
    if (op == T_OR) {
        if (truthy(leftValue)) {
            return makeBool(true);
        }
        return makeBool(truthy(right->eval(interpreter)));
    }
    throw LangError("Unsupported logical operator");
}

Value CallExpr::eval(Interpreter& interpreter) {
    Value target = callee->eval(interpreter);
    std::vector<Value> args;
    args.reserve(arguments.size());
    for (const ExprPtr& argument : arguments) {
        args.push_back(argument->eval(interpreter));
    }

    if (target.type != ValueType::Function || !target.fval) {
        throw LangError("Attempt to call a non-function value");
    }
    return interpreter.callFunctionValue(*target.fval, args);
}

Value FunctionExpr::eval(Interpreter& interpreter) {
    FunctionValue function;
    function.parameters = parameters;
    function.body = body;
    function.closure = interpreter.environment;
    return makeFunctionValue(function);
}

ExecResult EmptyStmt::exec(Interpreter&) {
    return ExecResult{};
}

ExecResult ExprStmt::exec(Interpreter& interpreter) {
    expression->eval(interpreter);
    return ExecResult{};
}

ExecResult VarDeclStmt::exec(Interpreter& interpreter) {
    if (isConst && !initializer) {
        throw LangError("Constant must be initialized: " + name);
    }
    if (isVar && !initializer) {
        throw LangError("Variable with var must be initialized: " + name);
    }
    Value value = initializer ? initializer->eval(interpreter) : defaultValue(declaredType);
    if (isConst) {
        interpreter.environment->defineConst(name, value);
    } else if (isVar) {
        interpreter.environment->defineInferred(name, value);
    } else {
        interpreter.environment->define(name, value);
    }
    return ExecResult{};
}

ExecResult BlockStmt::exec(Interpreter& interpreter) {
    auto scope = std::make_shared<Environment>(interpreter.environment);
    return interpreter.executeBlock(statements, scope);
}

ExecResult IfStmt::exec(Interpreter& interpreter) {
    if (truthy(condition->eval(interpreter))) {
        return thenBranch->exec(interpreter);
    }
    if (elseBranch) {
        return elseBranch->exec(interpreter);
    }
    return ExecResult{};
}

ExecResult WhileStmt::exec(Interpreter& interpreter) {
    while (!condition || truthy(condition->eval(interpreter))) {
        ExecResult result = body->exec(interpreter);
        if (result.flow == Flow::Return) {
            return result;
        }
        if (result.flow == Flow::Break) {
            break;
        }
    }
    return ExecResult{};
}

ExecResult ForStmt::exec(Interpreter& interpreter) {
    if (initializer) {
        initializer->exec(interpreter);
    }

    while (true) {
        if (condition && !truthy(condition->eval(interpreter))) {
            break;
        }
        ExecResult result = body->exec(interpreter);
        if (result.flow == Flow::Return) {
            return result;
        }
        if (result.flow == Flow::Break) {
            break;
        }
        if (increment) {
            increment->eval(interpreter);
        }
    }
    return ExecResult{};
}

ExecResult ReturnStmt::exec(Interpreter& interpreter) {
    ExecResult result;
    result.flow = Flow::Return;
    result.value = value ? value->eval(interpreter) : makeVoid();
    return result;
}

ExecResult BreakStmt::exec(Interpreter&) {
    ExecResult result;
    result.flow = Flow::Break;
    return result;
}

ExecResult ContinueStmt::exec(Interpreter&) {
    ExecResult result;
    result.flow = Flow::Continue;
    return result;
}

ExecResult FunctionDeclStmt::exec(Interpreter& interpreter) {
    FunctionValue function;
    for (const auto& parameter : parameters) {
        function.parameters.push_back(parameter.second);
    }
    function.body = body;
    if (interpreter.environment != interpreter.globals) {
        function.closure = interpreter.environment;
    }
    interpreter.functions[name] = std::move(function);
    return ExecResult{};
}

Interpreter::Interpreter() {
    globals->isGlobal = true;

    natives["print"] = [](Interpreter&, const std::vector<Value>& args) {
        for (const Value& arg : args) {
            std::cout << valueToString(arg);
        }
        return makeVoid();
    };

    natives["echo"] = natives["print"];

    natives["println"] = [](Interpreter&, const std::vector<Value>& args) {
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) {
                std::cout << ' ';
            }
            std::cout << valueToString(args[i]);
        }
        std::cout << '\n';
        return makeVoid();
    };
}

ExecResult Interpreter::executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> env) {
    EnvGuard guard(*this);
    environment = std::move(env);

    for (const StmtPtr& statement : statements) {
        ExecResult result = statement->exec(*this);
        if (result.flow != Flow::Normal) {
            return result;
        }
    }
    return ExecResult{};
}

Value Interpreter::callFunctionValue(const FunctionValue& function, const std::vector<Value>& args) {
    if (!function.nativeName.empty()) {
        auto nativeIt = natives.find(function.nativeName);
        if (nativeIt == natives.end()) {
            throw LangError("Undefined function: " + function.nativeName);
        }
        return nativeIt->second(*this, args);
    }

    if (!function.body) {
        throw LangError("Invalid function value");
    }

    if (args.size() != function.parameters.size()) {
        throw LangError("Function expects " + std::to_string(function.parameters.size()) +
                        " arguments but got " + std::to_string(args.size()));
    }

    auto parent = function.closure ? function.closure : globals;
    auto scope = std::make_shared<Environment>(parent);
    for (std::size_t i = 0; i < args.size(); ++i) {
        scope->define(function.parameters[i], args[i]);
    }

    ExecResult result = executeBlock(function.body->statements, scope);
    if (result.flow == Flow::Return) {
        return result.value;
    }
    return makeVoid();
}

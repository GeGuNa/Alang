#include "interpreter.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>
#include <utility>

Value resolveObject(const Value& value);

const FunctionValue* findMethod(const ClassInfo* info, const std::string& name) {
    for (const ClassInfo* current = info; current != nullptr; current = current->parent.get()) {
        auto it = current->methods.find(name);
        if (it != current->methods.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

const Value* findConstant(const ClassInfo* info, const std::string& name) {
    for (const ClassInfo* current = info; current != nullptr; current = current->parent.get()) {
        auto it = current->constants.find(name);
        if (it != current->constants.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

const Value* findDefaultProperty(const ClassInfo* info, const std::string& name) {
    for (const ClassInfo* current = info; current != nullptr; current = current->parent.get()) {
        auto it = current->defaultProperties.find(name);
        if (it != current->defaultProperties.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

std::string findPropertyVisibility(const ClassInfo* info, const std::string& name) {
    for (const ClassInfo* current = info; current != nullptr; current = current->parent.get()) {
        auto it = current->propertyVisibility.find(name);
        if (it != current->propertyVisibility.end()) {
            return it->second;
        }
    }
    return "public";
}

bool truthy(const Value& value) {
    switch (value.type) {
        case ValueType::Void: return false;
        case ValueType::Null: return false;
        case ValueType::Int: return value.ival != 0;
        case ValueType::Double: return value.dval != 0.0;
        case ValueType::Bool: return value.bval;
        case ValueType::String: return !value.sval.empty();
        case ValueType::Function: return static_cast<bool>(value.fval);
        case ValueType::Object: return static_cast<bool>(value.oval);
        case ValueType::Class: return static_cast<bool>(value.cval);
        case ValueType::Namespace: return static_cast<bool>(value.nval);
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
        case ValueType::Null: return 0.0;
        case ValueType::Function: return 0.0;
        case ValueType::Object: return 0.0;
        case ValueType::Class: return 0.0;
        case ValueType::Namespace: return 0.0;
    }
    return 0.0;
}

std::string valueToString(const Value& value) {
    switch (value.type) {
        case ValueType::Void: return "void";
        case ValueType::Null: return "null";
        case ValueType::Int: return std::to_string(value.ival);
        case ValueType::Double: {
            std::ostringstream stream;
            stream << value.dval;
            return stream.str();
        }
        case ValueType::Bool: return value.bval ? "true" : "false";
        case ValueType::String: return value.sval;
        case ValueType::Function: return "<function>";
        case ValueType::Object: {
            Value resolved = resolveObject(value);
            if (resolved.oval && resolved.oval->classInfo) {
                return "<" + resolved.oval->classInfo->name + " object>";
            }
            return "<object>";
        }
        case ValueType::Class:
            if (value.cval) {
                return "class " + value.cval->name;
            }
            return "<class>";
        case ValueType::Namespace:
            if (value.nval) {
                return "namespace " + value.nval->name;
            }
            return "<namespace>";
    }
    return "";
}

bool valuesEqual(const Value& left, const Value& right) {
    if (left.type == ValueType::Object || right.type == ValueType::Object) {
        Value a = resolveObject(left);
        Value b = resolveObject(right);
        return a.oval == b.oval;
    }
    if (left.type == ValueType::Class || right.type == ValueType::Class) {
        return left.cval == right.cval;
    }
    if (left.type == ValueType::Namespace || right.type == ValueType::Namespace) {
        return left.nval == right.nval;
    }
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
    if (left.type == ValueType::Null || right.type == ValueType::Null) {
        return left.type == right.type;
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

std::vector<Value> evaluateArguments(const std::vector<ExprPtr>& expressions, Interpreter& interpreter) {
    std::vector<Value> args;
    args.reserve(expressions.size());
    for (const ExprPtr& expression : expressions) {
        args.push_back(expression->eval(interpreter));
    }
    return args;
}

Value makeObjectValue(const std::shared_ptr<ObjectInstance>& object) {
    Value value;
    value.type = ValueType::Object;
    value.oval = object;
    return value;
}

Value makeNamespaceValue(const std::shared_ptr<NamespaceInfo>& info) {
    Value value;
    value.type = ValueType::Namespace;
    value.nval = info;
    return value;
}

std::shared_ptr<ClassInfo> findClassInfo(const Interpreter& interpreter, const std::string& name) {
    auto it = interpreter.classes.find(name);
    if (it != interpreter.classes.end()) {
        return it->second;
    }
    for (const auto& entry : interpreter.namespaces) {
        auto inner = entry.second->classes.find(name);
        if (inner != entry.second->classes.end()) {
            return inner->second;
        }
    }
    for (const auto& used : interpreter.usingNamespaces) {
        auto inner = used->classes.find(name);
        if (inner != used->classes.end()) {
            return inner->second;
        }
    }
    return nullptr;
}

std::shared_ptr<NamespaceInfo> findSimpleNamespace(Interpreter& interpreter, const std::string& name) {
    auto it = interpreter.namespaces.find(name);
    if (it != interpreter.namespaces.end()) {
        return it->second;
    }
    for (const auto& used : interpreter.usingNamespaces) {
        auto inner = used->namespaces.find(name);
        if (inner != used->namespaces.end()) {
            return inner->second;
        }
    }
    return nullptr;
}

std::shared_ptr<NamespaceInfo> findNamespace(Interpreter& interpreter, const std::string& name) {
    std::size_t start = 0;
    std::size_t separator = name.find("::", start);
    std::string first = separator == std::string::npos ? name : name.substr(start, separator - start);
    std::shared_ptr<NamespaceInfo> result = findSimpleNamespace(interpreter, first);
    if (!result) {
        return nullptr;
    }
    while (separator != std::string::npos) {
        start = separator + 2;
        separator = name.find("::", start);
        std::string part = separator == std::string::npos
            ? name.substr(start)
            : name.substr(start, separator - start);
        auto it = result->namespaces.find(part);
        if (it == result->namespaces.end()) {
            return nullptr;
        }
        result = it->second;
    }
    return result;
}

Value resolveObject(const Value& value) {
    if (value.type == ValueType::Object && !value.oval && !value.woval.expired()) {
        Value resolved = value;
        resolved.oval = value.woval.lock();
        return resolved;
    }
    return value;
}

Value storeObject(const Value& value) {
    if (value.type == ValueType::Object && value.oval) {
        Value stored;
        stored.type = ValueType::Object;
        stored.woval = value.oval;
        return stored;
    }
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

    auto classIt = interpreter.classes.find(name);
    if (classIt != interpreter.classes.end()) {
        Value classValue;
        classValue.type = ValueType::Class;
        classValue.cval = classIt->second;
        return classValue;
    }

    for (const auto& used : interpreter.usingNamespaces) {
        auto functionIt = used->functions.find(name);
        if (functionIt != used->functions.end()) {
            return makeFunctionValue(functionIt->second);
        }
        auto classInside = used->classes.find(name);
        if (classInside != used->classes.end()) {
            Value classValue;
            classValue.type = ValueType::Class;
            classValue.cval = classInside->second;
            return classValue;
        }
        auto constantIt = used->constants.find(name);
        if (constantIt != used->constants.end()) {
            return constantIt->second;
        }
    }

    auto namespaceInfo = findNamespace(interpreter, name);
    if (namespaceInfo) {
        return makeNamespaceValue(namespaceInfo);
    }

    throw LangError("Undefined variable: " + name);
}

void VarExpr::assign(Interpreter& interpreter, const Value& value) {
    if (interpreter.environment->assign(name, value)) {
        return;
    }
    if (fromDollar) {
        interpreter.environment->defineInferred(name, value);
        return;
    }
    throw LangError("Undefined variable: " + name);
}

Value AssignExpr::eval(Interpreter& interpreter) {
    Value value = this->value->eval(interpreter);
    target->assign(interpreter, value);
    return value;
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

Value ThisExpr::eval(Interpreter& interpreter) {
    Value value = interpreter.currentThis();
    if (value.type != ValueType::Object) {
        throw LangError("this is only available inside a method");
    }
    return value;
}

Value ParentExpr::eval(Interpreter& interpreter) {
    std::string className = interpreter.currentClass();
    std::shared_ptr<ClassInfo> info = findClassInfo(interpreter, className);
    if (!info || !info->parent) {
        throw LangError("parent is not available in this class");
    }
    Value value;
    value.type = ValueType::Class;
    value.cval = info->parent;
    return value;
}

Value NewExpr::eval(Interpreter& interpreter) {
    std::shared_ptr<ClassInfo> classInfo = findClassInfo(interpreter, className);
    if (!classInfo) {
        throw LangError("Undefined class: " + className);
    }

    std::vector<Value> args = evaluateArguments(arguments, interpreter);
    bool hasConstructor = findMethod(classInfo.get(), "__construct") != nullptr;
    if (!hasConstructor && !args.empty()) {
        throw LangError("Class " + className + " has no constructor");
    }

    std::weak_ptr<Interpreter> weakInterpreter = interpreter.weak_from_this();
    std::shared_ptr<ObjectInstance> object(new ObjectInstance(), [weakInterpreter](ObjectInstance* raw) {
        if (raw->constructed && !raw->destructed) {
            raw->destructed = true;
            if (std::shared_ptr<Interpreter> owner = weakInterpreter.lock()) {
                if (!owner->shuttingDown && raw->classInfo &&
                    findMethod(raw->classInfo.get(), "__destruct") != nullptr) {
                    Value self;
                    self.type = ValueType::Object;
                    self.oval = std::shared_ptr<ObjectInstance>(raw, [](ObjectInstance*) {});
                    try {
                        owner->callMethod(self, "__destruct", {});
                    } catch (...) {
                    }
                }
            }
        }
        delete raw;
    });

    object->classInfo = classInfo;
    object->properties = object->classInfo->defaultProperties;
    Value instance = makeObjectValue(object);

    if (hasConstructor) {
        interpreter.callMethod(instance, "__construct", args);
    }
    object->constructed = true;
    return instance;
}

Value MemberExpr::eval(Interpreter& interpreter) {
    Value object = resolveObject(this->object->eval(interpreter));
    if (object.type != ValueType::Object || !object.oval) {
        throw LangError("Attempt to access a member on a non-object");
    }

    const std::shared_ptr<ClassInfo>& info = object.oval->classInfo;
    std::string visibility = findPropertyVisibility(info.get(), name);
    interpreter.checkVisibility(info->name, visibility);

    auto it = object.oval->properties.find(name);
    if (it == object.oval->properties.end()) {
        throw LangError("Undefined property: " + info->name + "->" + name);
    }
    return it->second;
}

void MemberExpr::assign(Interpreter& interpreter, const Value& value) {
    Value object = resolveObject(this->object->eval(interpreter));
    if (object.type != ValueType::Object || !object.oval) {
        throw LangError("Attempt to set a member on a non-object");
    }

    const std::shared_ptr<ClassInfo>& info = object.oval->classInfo;
    std::string visibility = findPropertyVisibility(info.get(), name);
    interpreter.checkVisibility(info->name, visibility);

    auto it = object.oval->properties.find(name);
    if (it == object.oval->properties.end()) {
        throw LangError("Undefined property: " + info->name + "->" + name);
    }
    it->second = storeObject(value);
}

Value StaticMemberExpr::eval(Interpreter& interpreter) {
    Value owner = classExpr->eval(interpreter);

    if (owner.type == ValueType::Namespace && owner.nval) {
        const std::shared_ptr<NamespaceInfo>& info = owner.nval;
        auto nested = info->namespaces.find(name);
        if (nested != info->namespaces.end()) {
            return makeNamespaceValue(nested->second);
        }
        auto classIt = info->classes.find(name);
        if (classIt != info->classes.end()) {
            Value value;
            value.type = ValueType::Class;
            value.cval = classIt->second;
            return value;
        }
        auto functionIt = info->functions.find(name);
        if (functionIt != info->functions.end()) {
            return makeFunctionValue(functionIt->second);
        }
        auto constantIt = info->constants.find(name);
        if (constantIt != info->constants.end()) {
            return constantIt->second;
        }
        throw LangError("Undefined namespace member: " + info->name + "::" + name);
    }

    if (owner.type != ValueType::Class || !owner.cval) {
        throw LangError("Attempt to access a static member on a non-class");
    }

    const Value* constant = findConstant(owner.cval.get(), name);
    if (constant) {
        return *constant;
    }
    throw LangError("Undefined static member: " + owner.cval->name + "::" + name);
}

Value MemberCallExpr::eval(Interpreter& interpreter) {
    Value object = resolveObject(this->object->eval(interpreter));
    return interpreter.callMethod(object, name, evaluateArguments(arguments, interpreter));
}

Value StaticCallExpr::eval(Interpreter& interpreter) {
    Value owner = classExpr->eval(interpreter);
    std::vector<Value> args = evaluateArguments(arguments, interpreter);

    if (owner.type == ValueType::Namespace && owner.nval) {
        auto it = owner.nval->functions.find(name);
        if (it == owner.nval->functions.end()) {
            throw LangError("Undefined namespace function: " + owner.nval->name + "::" + name);
        }
        return interpreter.callFunctionValue(it->second, args);
    }

    if (owner.type != ValueType::Class || !owner.cval) {
        throw LangError("Attempt to call a static method on a non-class");
    }

    const FunctionValue* method = findMethod(owner.cval.get(), name);
    if (method && !method->isStatic && interpreter.currentThis().type == ValueType::Object) {
        return interpreter.callMethodFrom(owner.cval, interpreter.currentThis(), name, args);
    }
    return interpreter.callStaticMethod(owner.cval, name, args);
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
    if (isConst && interpreter.currentNamespace) {
        interpreter.currentNamespace->constants[name] = value;
    }
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
    if (interpreter.currentNamespace) {
        interpreter.currentNamespace->functions[name] = std::move(function);
    } else {
        interpreter.functions[name] = std::move(function);
    }
    return ExecResult{};
}

ExecResult ClassDeclStmt::exec(Interpreter& interpreter) {
    auto info = std::make_shared<ClassInfo>();
    info->name = name;

    if (!baseName.empty()) {
        auto parentIt = interpreter.classes.find(baseName);
        if (parentIt == interpreter.classes.end() && interpreter.currentNamespace) {
            parentIt = interpreter.currentNamespace->classes.find(baseName);
        }
        if (parentIt == interpreter.classes.end() || !parentIt->second) {
            throw LangError("Undefined base class: " + baseName);
        }
        info->parent = parentIt->second;
        for (const ClassInfo* current = info->parent.get(); current != nullptr; current = current->parent.get()) {
            if (current->name == name) {
                throw LangError("Circular inheritance involving class: " + name);
            }
        }
        info->defaultProperties = info->parent->defaultProperties;
        info->propertyVisibility = info->parent->propertyVisibility;
        info->constants = info->parent->constants;
    }

    for (const ClassPropertyNode& property : properties) {
        Value value = property.initializer
            ? property.initializer->eval(interpreter)
            : defaultValue(property.declaredType);
        if (property.isStatic) {
            info->constants[property.name] = value;
        } else {
            info->defaultProperties[property.name] = storeObject(value);
            info->propertyVisibility[property.name] = property.visibility;
        }
    }

    for (const ClassConstantNode& constant : constants) {
        info->constants[constant.name] = constant.initializer->eval(interpreter);
    }

    for (const ClassMethodNode& method : methods) {
        FunctionValue function;
        function.parameters = method.parameters;
        function.body = method.body;
        function.className = name;
        function.visibility = method.visibility;
        function.isStatic = method.isStatic;
        info->methods[method.name] = std::move(function);
    }

    if (interpreter.currentNamespace) {
        interpreter.currentNamespace->classes[name] = info;
    } else {
        interpreter.classes[name] = info;
    }
    return ExecResult{};
}

struct NamespaceGuard {
    Interpreter& interpreter;
    std::shared_ptr<NamespaceInfo> saved;

    NamespaceGuard(Interpreter& i, const std::shared_ptr<NamespaceInfo>& info)
        : interpreter(i), saved(i.currentNamespace) {
        interpreter.currentNamespace = info;
    }

    ~NamespaceGuard() {
        interpreter.currentNamespace = saved;
    }
};

ExecResult NamespaceStmt::exec(Interpreter& interpreter) {
    std::shared_ptr<NamespaceInfo> info;
    auto it = interpreter.namespaces.find(name);
    if (it != interpreter.namespaces.end()) {
        info = it->second;
    } else {
        info = std::make_shared<NamespaceInfo>();
        info->name = name;
        info->parent = interpreter.currentNamespace;
        interpreter.namespaces[name] = info;
        if (interpreter.currentNamespace) {
            interpreter.currentNamespace->namespaces[name] = info;
        }
    }

    NamespaceGuard guard(interpreter, info);
    auto scope = std::make_shared<Environment>(interpreter.environment);
    return interpreter.executeBlock(statements, scope);
}

ExecResult UsingNamespaceStmt::exec(Interpreter& interpreter) {
    auto info = findNamespace(interpreter, name);
    if (!info) {
        throw LangError("Undefined namespace: " + name);
    }
    interpreter.usingNamespaces.push_back(info);
    return ExecResult{};
}

Interpreter::~Interpreter() {
    shuttingDown = true;
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

void collectLiveObjects(const Value& value, std::vector<std::shared_ptr<ObjectInstance>>& objects,
                        std::unordered_set<ObjectInstance*>& seen) {
    Value resolved = resolveObject(value);
    if (resolved.type == ValueType::Object && resolved.oval && !resolved.oval->destructed &&
        seen.insert(resolved.oval.get()).second) {
        objects.push_back(resolved.oval);
    }
}

void Interpreter::shutdown() {
    thisStack.clear();
    thisClassStack.clear();

    for (int pass = 0; pass < 8; ++pass) {
        std::vector<std::shared_ptr<ObjectInstance>> objects;
        std::unordered_set<ObjectInstance*> seenObjects;
        std::unordered_set<Environment*> seenEnvironments;
        std::vector<Environment*> environments;
        environments.push_back(globals.get());

        while (!environments.empty()) {
            Environment* env = environments.back();
            environments.pop_back();
            if (!env || !seenEnvironments.insert(env).second) {
                continue;
            }
            for (const auto& entry : env->values) {
                collectLiveObjects(entry.second, objects, seenObjects);
            }
            if (env->parent) {
                environments.push_back(env->parent.get());
            }
            if (auto global = env->globalParent.lock()) {
                environments.push_back(global.get());
            }
        }

        for (const auto& entry : classes) {
            if (!entry.second) {
                continue;
            }
            for (const auto& constant : entry.second->constants) {
                collectLiveObjects(constant.second, objects, seenObjects);
            }
        }

        for (const auto& entry : namespaces) {
            if (!entry.second) {
                continue;
            }
            for (const auto& constant : entry.second->constants) {
                collectLiveObjects(constant.second, objects, seenObjects);
            }
        }

        if (objects.empty()) {
            break;
        }

        for (const auto& object : objects) {
            if (!object->constructed || object->destructed) {
                continue;
            }
            object->destructed = true;
            if (!object->classInfo || findMethod(object->classInfo.get(), "__destruct") == nullptr) {
                continue;
            }
            Value self;
            self.type = ValueType::Object;
            self.oval = object;
            try {
                callMethod(self, "__destruct", {});
            } catch (...) {
            }
        }
    }
    shuttingDown = true;
}

struct ThisGuard {
    Interpreter& interpreter;

    ThisGuard(Interpreter& i, const Value& object, const std::string& className) : interpreter(i) {
        interpreter.thisStack.push_back(object);
        interpreter.thisClassStack.push_back(className);
    }

    ~ThisGuard() {
        interpreter.thisStack.pop_back();
        interpreter.thisClassStack.pop_back();
    }
};

Value Interpreter::currentThis() const {
    if (thisStack.empty()) {
        return makeVoid();
    }
    return thisStack.back();
}

std::string Interpreter::currentClass() const {
    if (thisClassStack.empty()) {
        return "";
    }
    return thisClassStack.back();
}

bool Interpreter::isSubclassOf(const std::string& child, const std::string& parentName) const {
    if (child == parentName) {
        return true;
    }
    std::shared_ptr<ClassInfo> childInfo = findClassInfo(*this, child);
    if (!childInfo) {
        return false;
    }
    for (const ClassInfo* current = childInfo->parent.get(); current != nullptr; current = current->parent.get()) {
        if (current->name == parentName) {
            return true;
        }
    }
    return false;
}

void Interpreter::checkVisibility(const std::string& ownerClass, const std::string& visibility) const {
    if (visibility == "public") {
        return;
    }
    if (isSubclassOf(currentClass(), ownerClass)) {
        return;
    }
    throw LangError("Cannot access " + visibility + " member of " + ownerClass);
}

Value Interpreter::callMethod(const Value& objectValue, const std::string& name, const std::vector<Value>& args) {
    Value object = resolveObject(objectValue);
    if (object.type != ValueType::Object || !object.oval) {
        throw LangError("Attempt to call a method on a non-object");
    }
    return callMethodFrom(object.oval->classInfo, object, name, args);
}

Value Interpreter::callMethodFrom(const std::shared_ptr<ClassInfo>& info, const Value& object,
                                  const std::string& name, const std::vector<Value>& args) {
    const FunctionValue* found = findMethod(info.get(), name);
    if (!found) {
        throw LangError("Undefined method: " + info->name + "::" + name);
    }
    const FunctionValue& function = *found;
    checkVisibility(info->name, function.visibility);

    if (args.size() != function.parameters.size()) {
        throw LangError("Method " + name + " expects " +
                        std::to_string(function.parameters.size()) +
                        " arguments but got " + std::to_string(args.size()));
    }

    auto parent = function.closure ? function.closure : globals;
    auto scope = std::make_shared<Environment>(parent);
    for (std::size_t i = 0; i < args.size(); ++i) {
        scope->define(function.parameters[i], args[i]);
    }

    std::string actualClass = info->name;
    if (object.type == ValueType::Object && object.oval && object.oval->classInfo) {
        actualClass = object.oval->classInfo->name;
    }
    ThisGuard guard(*this, object, actualClass);
    ExecResult result = executeBlock(function.body->statements, scope);
    if (result.flow == Flow::Return) {
        return result.value;
    }
    return makeVoid();
}

Value Interpreter::callStaticMethod(const std::shared_ptr<ClassInfo>& info, const std::string& name,
                                    const std::vector<Value>& args) {
    const FunctionValue* found = findMethod(info.get(), name);
    if (!found) {
        throw LangError("Undefined method: " + info->name + "::" + name);
    }
    const FunctionValue& function = *found;
    checkVisibility(info->name, function.visibility);

    if (args.size() != function.parameters.size()) {
        throw LangError("Method " + name + " expects " +
                        std::to_string(function.parameters.size()) +
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

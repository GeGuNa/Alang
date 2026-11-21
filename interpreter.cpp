#include "interpreter.h"
#include "http.h"
#include "mysql.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

Value resolveObject(const Value& value);
void httpRunHandler(Interpreter&, const FunctionValue&, HttpRequestData&, HttpResponseData&);

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
        case ValueType::String: return !value.str().empty();
        case ValueType::Function: return static_cast<bool>(value.fnPtr());
        case ValueType::Object: return static_cast<bool>(value.objPtr());
        case ValueType::ObjectWeak: {
            std::shared_ptr<WeakObjectRef> ref = value.weakRef();
            return ref && !ref->ref.expired();
        }
        case ValueType::Class: return static_cast<bool>(value.clsPtr());
        case ValueType::Namespace: return static_cast<bool>(value.nsPtr());
        case ValueType::Future: return static_cast<bool>(value.futurePtr());
        case ValueType::Thread: return static_cast<bool>(value.threadPtr());
        case ValueType::HttpServer: return static_cast<bool>(value.serverPtr());
        case ValueType::HttpRequest: return static_cast<bool>(value.requestPtr());
        case ValueType::HttpResponse: return static_cast<bool>(value.responsePtr());
        case ValueType::UploadedFile: return static_cast<bool>(value.uploadPtr());
        case ValueType::Array: return static_cast<bool>(value.arrayPtr());
        case ValueType::Map: return static_cast<bool>(value.mapPtr());
        case ValueType::Exception: return static_cast<bool>(value.exceptionPtr());
        case ValueType::MysqlConn: return static_cast<bool>(value.mysqlPtr());
        case ValueType::MysqlResult: return static_cast<bool>(value.mysqlResultPtr());
        case ValueType::MysqlRow: return static_cast<bool>(value.mysqlRowPtr());
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
                return std::stod(value.str());
            } catch (const std::exception&) {
                return 0.0;
            }
        case ValueType::Void: return 0.0;
        case ValueType::Null: return 0.0;
        case ValueType::Function: return 0.0;
        case ValueType::Object: return 0.0;
        case ValueType::ObjectWeak: return 0.0;
        case ValueType::Class: return 0.0;
        case ValueType::Namespace: return 0.0;
        case ValueType::Future: return 0.0;
        case ValueType::Thread: return 0.0;
        case ValueType::HttpServer: return 0.0;
        case ValueType::HttpRequest: return 0.0;
        case ValueType::HttpResponse: return 0.0;
        case ValueType::UploadedFile: return 0.0;
        case ValueType::Array: return 0.0;
        case ValueType::Map: return 0.0;
        case ValueType::Exception: return 0.0;
        case ValueType::MysqlConn: return 0.0;
        case ValueType::MysqlResult: return 0.0;
        case ValueType::MysqlRow: return 0.0;
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
        case ValueType::String: return value.str();
        case ValueType::Function: return "<function>";
        case ValueType::ObjectWeak: {
            Value resolved = resolveObject(value);
            if (resolved.objPtr() && resolved.objPtr()->classInfo) {
                return "<" + resolved.objPtr()->classInfo->name + " object>";
            }
            return "<object>";
        }
        case ValueType::Object: {
            Value resolved = resolveObject(value);
            if (resolved.objPtr() && resolved.objPtr()->classInfo) {
                return "<" + resolved.objPtr()->classInfo->name + " object>";
            }
            return "<object>";
        }
        case ValueType::Class:
            if (value.clsPtr()) {
                return "class " + value.clsPtr()->name;
            }
            return "<class>";
        case ValueType::Namespace:
            if (value.nsPtr()) {
                return "namespace " + value.nsPtr()->name;
            }
            return "<namespace>";
        case ValueType::Future: return "<future>";
        case ValueType::Thread: return "<thread>";
        case ValueType::HttpServer: return "<server>";
        case ValueType::HttpRequest: return "<request>";
        case ValueType::HttpResponse: return "<response>";
        case ValueType::UploadedFile: return "<file>";
        case ValueType::Array: return "<array>";
        case ValueType::Map: return "<map>";
        case ValueType::Exception: {
            std::shared_ptr<ExceptionData> error = value.exceptionPtr();
            return error ? "Exception: " + error->message : "Exception";
        }
        case ValueType::MysqlConn: return "<mysql>";
        case ValueType::MysqlResult: return "<mysql result>";
        case ValueType::MysqlRow: return "<mysql row>";
    }
    return "";
}

bool valuesEqual(const Value& left, const Value& right) {
    if (left.type == ValueType::Object || right.type == ValueType::Object ||
        left.type == ValueType::ObjectWeak || right.type == ValueType::ObjectWeak) {
        Value a = resolveObject(left);
        Value b = resolveObject(right);
        return a.objPtr() == b.objPtr();
    }
    if (left.type == ValueType::Class || right.type == ValueType::Class) {
        return left.clsPtr() == right.clsPtr();
    }
    if (left.type == ValueType::Namespace || right.type == ValueType::Namespace) {
        return left.nsPtr() == right.nsPtr();
    }
    if (left.type == ValueType::Future || right.type == ValueType::Future) {
        return left.futurePtr() == right.futurePtr();
    }
    if (left.type == ValueType::Thread || right.type == ValueType::Thread) {
        return left.threadPtr() == right.threadPtr();
    }
    if (left.type == ValueType::Function || right.type == ValueType::Function) {
        return left.fnPtr() == right.fnPtr();
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
        if (left.str() < right.str()) return -1;
        if (left.str() > right.str()) return 1;
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
    if (value.type == ValueType::Function && value.fnPtr() && value.fnPtr()->closure.get() == scope) {
        value.fnPtr()->closure.reset();
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

Value* Environment::cachedValue(const std::string& name) const {
    for (std::size_t i = 0; i < cacheValues.size(); ++i) {
        if (cacheValues[i] != nullptr && cacheKeys[i] == name) {
            return cacheValues[i];
        }
    }
    return nullptr;
}

void Environment::rememberValue(const std::string& name, Value* value) const {
    std::size_t slot = cacheCursor % cacheValues.size();
    cacheKeys[slot] = name;
    cacheValues[slot] = value;
    ++cacheCursor;
}

bool Environment::get(const std::string& name, Value& out) const {
    if (Value* cached = cachedValue(name)) {
        out = *cached;
        return true;
    }

    const Environment* current = this;
    std::shared_ptr<Environment> held;
    while (current != nullptr) {
        auto it = current->values.find(name);
        if (it != current->values.end()) {
            if (current == this) {
                rememberValue(name, const_cast<Value*>(&it->second));
            }
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
    if (Value* cached = cachedValue(name)) {
        if (constants.count(name) != 0) {
            throw LangError("Cannot assign to constant: " + name);
        }
        auto typeIt = inferredTypes.find(name);
        if (typeIt != inferredTypes.end() && typeIt->second != value.type) {
            throw LangError("Type mismatch for variable: " + name);
        }
        *cached = value;
        return true;
    }

    Environment* current = this;
    std::shared_ptr<Environment> held;
    while (current != nullptr) {
        auto it = current->values.find(name);
        if (it != current->values.end()) {
            if (current == this) {
                rememberValue(name, &it->second);
            }
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
    value.payload = std::make_shared<FunctionValue>(function);
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
    value.payload = object;
    return value;
}

Value makeNamespaceValue(const std::shared_ptr<NamespaceInfo>& info) {
    Value value;
    value.type = ValueType::Namespace;
    value.payload = info;
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
    if (value.type == ValueType::ObjectWeak) {
        std::shared_ptr<WeakObjectRef> ref = value.weakRef();
        std::shared_ptr<ObjectInstance> locked = ref ? ref->ref.lock() : nullptr;
        if (locked) {
            Value resolved;
            resolved.type = ValueType::Object;
            resolved.payload = locked;
            return resolved;
        }
        return makeNull();
    }
    return value;
}

bool ownsTransitively(const ObjectInstance* from, const ObjectInstance* target) {
    if (!from || !target) return false;
    if (from == target) return true;
    for (const auto& child : from->owned) {
        if (ownsTransitively(child.get(), target)) return true;
    }
    return false;
}

Value storeObject(const Value& value, ObjectInstance* owner) {
    if (value.type != ValueType::Object || !value.objPtr() || !owner) {
        return value;
    }
    if (ownsTransitively(value.objPtr().get(), owner)) {
        Value weak;
        weak.type = ValueType::ObjectWeak;
        auto ref = std::make_shared<WeakObjectRef>();
        ref->ref = value.objPtr();
        weak.payload = ref;
        return weak;
    }
    owner->owned.push_back(value.objPtr());
    return value;
}

Value storeObject(const Value& value) {
    return storeObject(value, nullptr);
}

Value LiteralExpr::eval(Interpreter&) {
    return value;
}

Value resolveName(Interpreter& interpreter, const std::string& name) {
    auto nativeValueIt = interpreter.nativeValues.find(name);
    if (nativeValueIt != interpreter.nativeValues.end()) {
        return nativeValueIt->second;
    }

    auto functionIt = interpreter.functions.find(name);
    if (functionIt != interpreter.functions.end()) {
        return makeFunctionValue(functionIt->second);
    }

    auto classIt = interpreter.classes.find(name);
    if (classIt != interpreter.classes.end()) {
        Value classValue;
        classValue.type = ValueType::Class;
        classValue.payload = classIt->second;
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
            classValue.payload = classInside->second;
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

Value VarExpr::eval(Interpreter& interpreter) {
    Value value;
    if (interpreter.environment->get(name, value)) {
        return value;
    }
    auto slot = interpreter.globalSlots.find(name);
    if (slot != interpreter.globalSlots.end() &&
        slot->second >= 0 &&
        static_cast<std::size_t>(slot->second) < interpreter.vmGlobals.size()) {
        return interpreter.vmGlobals[static_cast<std::size_t>(slot->second)];
    }
    return resolveName(interpreter, name);
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

    if (target.type != ValueType::Function || !target.fnPtr()) {
        throw LangError("Attempt to call a non-function value");
    }
    return interpreter.callFunctionValue(*target.fnPtr(), args);
}

Value FunctionExpr::eval(Interpreter& interpreter) {
    FunctionValue function;
    function.parameters = parameters;
    function.body = body;
    function.closure = interpreter.environment;
    function.isAsync = isAsync;
    return makeFunctionValue(function);
}

Value AwaitExpr::eval(Interpreter& interpreter) {
    return interpreter.awaitValue(value->eval(interpreter));
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
    value.payload = info->parent;
    return value;
}

Value createObject(Interpreter& interpreter, const std::shared_ptr<ClassInfo>& classInfo,
                   const std::vector<Value>& args) {
    bool isThread = classInfo->name == "Thread";
    bool hasConstructor = findMethod(classInfo.get(), "__construct") != nullptr;
    if (!hasConstructor && !isThread && !args.empty()) {
        throw LangError("Class " + classInfo->name + " has no constructor");
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
                    self.payload = std::shared_ptr<ObjectInstance>(raw, [](ObjectInstance*) {});
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
    for (auto& property : object->properties) {
        if (property.second.type == ValueType::Object && property.second.objPtr()) {
            if (ownsTransitively(property.second.objPtr().get(), object.get())) {
                Value weak;
                weak.type = ValueType::ObjectWeak;
                auto ref = std::make_shared<WeakObjectRef>();
                ref->ref = property.second.objPtr();
                weak.payload = ref;
                property.second = weak;
            } else {
                object->owned.push_back(property.second.objPtr());
            }
        }
    }
    if (classInfo->name == "Thread" && !args.empty()) {
        object->properties["__fn"] = args[0];
    }
    if (classInfo->name == "MySQL") {
        Value connection;
        connection.type = ValueType::MysqlConn;
        connection.payload = std::make_shared<MysqlConnectionData>();
        return connection;
    }
    if (classInfo->name == "Server") {
        auto state = std::make_shared<HttpServerState>();
        state->owner = interpreter.weak_from_this();
        interpreter.serverStates.push_back(state);
        Value server;
        server.type = ValueType::HttpServer;
        server.payload = state;
        return server;
    }
    Value instance = makeObjectValue(object);

    if (hasConstructor) {
        interpreter.callMethod(instance, "__construct", args);
    }
    if (isThread && args.empty()) {
        object->properties["__fn"] = makeVoid();
    }
    object->constructed = true;
    return instance;
}

Value NewExpr::eval(Interpreter& interpreter) {
    std::shared_ptr<ClassInfo> classInfo = findClassInfo(interpreter, className);
    if (!classInfo) {
        throw LangError("Undefined class: " + className);
    }
    return createObject(interpreter, classInfo, evaluateArguments(arguments, interpreter));
}

Value MemberExpr::eval(Interpreter& interpreter) {
    Value object = resolveObject(this->object->eval(interpreter));
    if (object.type != ValueType::Object || !object.objPtr()) {
        throw LangError("Attempt to access a member on a non-object");
    }

    const std::shared_ptr<ClassInfo>& info = object.objPtr()->classInfo;
    std::string visibility = findPropertyVisibility(info.get(), name);
    interpreter.checkVisibility(info->name, visibility);

    auto it = object.objPtr()->properties.find(name);
    if (it == object.objPtr()->properties.end()) {
        throw LangError("Undefined property: " + info->name + "->" + name);
    }
    return it->second;
}

void MemberExpr::assign(Interpreter& interpreter, const Value& value) {
    Value object = resolveObject(this->object->eval(interpreter));
    if (object.type != ValueType::Object || !object.objPtr()) {
        throw LangError("Attempt to set a member on a non-object");
    }

    const std::shared_ptr<ClassInfo>& info = object.objPtr()->classInfo;
    std::string visibility = findPropertyVisibility(info.get(), name);
    interpreter.checkVisibility(info->name, visibility);

    auto it = object.objPtr()->properties.find(name);
    if (it == object.objPtr()->properties.end()) {
        throw LangError("Undefined property: " + info->name + "->" + name);
    }
    it->second = storeObject(value, object.objPtr().get());
}

void StaticMemberExpr::assign(Interpreter& interpreter, const Value& value) {
    Value owner = classExpr->eval(interpreter);
    if (owner.type == ValueType::Class && owner.clsPtr()) {
        owner.clsPtr()->constants[name] = value;
        return;
    }
    if (owner.type == ValueType::Namespace && owner.nsPtr()) {
        owner.nsPtr()->constants[name] = value;
        return;
    }
    throw LangError("Attempt to set a static member on a non-class");
}

Value StaticMemberExpr::eval(Interpreter& interpreter) {
    Value owner = classExpr->eval(interpreter);

    if (owner.type == ValueType::Namespace && owner.nsPtr()) {
        const std::shared_ptr<NamespaceInfo>& info = owner.nsPtr();
        auto nested = info->namespaces.find(name);
        if (nested != info->namespaces.end()) {
            return makeNamespaceValue(nested->second);
        }
        auto classIt = info->classes.find(name);
        if (classIt != info->classes.end()) {
            Value value;
            value.type = ValueType::Class;
            value.payload = classIt->second;
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

    if (owner.type != ValueType::Class || !owner.clsPtr()) {
        throw LangError("Attempt to access a static member on a non-class");
    }

    const Value* constant = findConstant(owner.clsPtr().get(), name);
    if (constant) {
        return *constant;
    }
    throw LangError("Undefined static member: " + owner.clsPtr()->name + "::" + name);
}

Value MemberCallExpr::eval(Interpreter& interpreter) {
    Value object = resolveObject(this->object->eval(interpreter));
    std::vector<Value> args = evaluateArguments(arguments, interpreter);
    int argc = static_cast<int>(args.size());
    bool handled = false;
    Value special = interpreter.threadMethod(object, name, args.data(), argc, handled);
    if (!handled) {
        special = interpreter.httpMethod(object, name, args.data(), argc, handled);
    }
    if (!handled) {
        special = interpreter.mysqlMethod(object, name, args.data(), argc, handled);
    }
    if (!handled) {
        special = interpreter.collectionMethod(object, name, args.data(), argc, handled);
    }
    if (handled) {
        return special;
    }
    return interpreter.callMethod(object, name, args);
}

Value StaticCallExpr::eval(Interpreter& interpreter) {
    Value owner = classExpr->eval(interpreter);
    std::vector<Value> args = evaluateArguments(arguments, interpreter);

    if (owner.type == ValueType::Namespace && owner.nsPtr()) {
        auto it = owner.nsPtr()->functions.find(name);
        if (it == owner.nsPtr()->functions.end()) {
            throw LangError("Undefined namespace function: " + owner.nsPtr()->name + "::" + name);
        }
        return interpreter.callFunctionValue(it->second, args);
    }

    if (owner.type != ValueType::Class || !owner.clsPtr()) {
        throw LangError("Attempt to call a static method on a non-class");
    }

    const FunctionValue* method = findMethod(owner.clsPtr().get(), name);
    if (method && !method->isStatic && interpreter.currentThis().type == ValueType::Object) {
        return interpreter.callMethodFrom(owner.clsPtr(), interpreter.currentThis(), name, args);
    }
    return interpreter.callStaticMethod(owner.clsPtr(), name, args);
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
    if (!needsScope) {
        return interpreter.executeBlock(statements, interpreter.environment);
    }
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
    info->isStruct = isStruct;

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
            info->defaultProperties[property.name] = value;
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
        function.isAsync = method.isAsync;
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

    natives["len"] = [](Interpreter&, const std::vector<Value>& args) {
        if (args.empty()) return makeInt(0);
        const Value& value = args[0];
        if (value.type == ValueType::Array) return makeInt(static_cast<long long>(value.arrayPtr()->items.size()));
        if (value.type == ValueType::Map) return makeInt(static_cast<long long>(value.mapPtr()->items.size()));
        if (value.type == ValueType::String) return makeInt(static_cast<long long>(value.str().size()));
        return makeInt(0);
    };

    for (const auto& entry : natives) {
        FunctionValue function;
        function.nativeName = entry.first;
        nativeValues[entry.first] = makeFunctionValue(function);
    }

    auto threadClass = std::make_shared<ClassInfo>();
    threadClass->name = "Thread";
    threadClass->defaultProperties["__fn"] = makeVoid();
    threadClass->defaultProperties["__future"] = makeVoid();
    threadClass->methods["start"] = FunctionValue{};
    threadClass->methods["join"] = FunctionValue{};
    classes["Thread"] = threadClass;

    auto serverClass = std::make_shared<ClassInfo>();
    serverClass->name = "Server";
    serverClass->methods["get"] = FunctionValue{};
    serverClass->methods["post"] = FunctionValue{};
    serverClass->methods["put"] = FunctionValue{};
    serverClass->methods["del"] = FunctionValue{};
    serverClass->methods["listen"] = FunctionValue{};
    serverClass->methods["port"] = FunctionValue{};
    serverClass->methods["secure"] = FunctionValue{};
    serverClass->methods["limit"] = FunctionValue{};
    serverClass->methods["connections"] = FunctionValue{};
    serverClass->methods["served"] = FunctionValue{};
    serverClass->methods["close"] = FunctionValue{};
    classes["Server"] = serverClass;

    auto mysqlClass = std::make_shared<ClassInfo>();
    mysqlClass->name = "MySQL";
    for (const char* method : {"connect", "close", "query", "escape", "lastError", "insertId", "affectedRows", "ping"}) {
        mysqlClass->methods[method] = FunctionValue{};
    }
    classes["MySQL"] = mysqlClass;

    httpSetHandlerHook(httpRunHandler);
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

Value Interpreter::callNative(const std::string& name, const Value* args, int argc) {
    auto it = natives.find(name);
    if (it == natives.end()) {
        throw LangError("Undefined function: " + name);
    }
    std::vector<Value> copied(args, args + argc);
    return it->second(*this, copied);
}

Value Interpreter::createObjectFrom(const std::shared_ptr<ClassInfo>& info, const Value* args, int argc) {
    std::vector<Value> copied(args, args + argc);
    return createObject(*this, info, copied);
}

Value Interpreter::invokeFunctionValue(const FunctionValue& function, const Value* args, int argc,
                                       const Value& self, const std::string& selfClass) {
    if (function.isAsync && self.type != ValueType::Object) {
        return spawnAsync(function, args, argc);
    }
    if (!function.nativeName.empty()) {
        return callNative(function.nativeName, args, argc);
    }
    if (function.chunk) {
        return executeChunk(*function.chunk, args, argc, self, selfClass);
    }
    std::vector<Value> copied(args, args + argc);
    return callFunctionValue(function, copied);
}

Value Interpreter::invokeMethod(const std::shared_ptr<ClassInfo>& info, const Value& object,
                                const std::string& name, const Value* args, int argc) {
    const FunctionValue* method = findMethod(info.get(), name);
    if (!method) throw LangError("Undefined method: " + info->name + "::" + name);
    checkVisibility(info->name, method->visibility);
    if (method->chunk) {
        return executeChunk(*method->chunk, args, argc, object, info->name);
    }
    std::vector<Value> copied(args, args + argc);
    return callMethodFrom(info, object, name, copied);
}

Value Interpreter::invokeStatic(const std::shared_ptr<ClassInfo>& info, const std::string& name,
                                const Value* args, int argc, const Value& self,
                                const std::string& selfClass) {
    const FunctionValue* method = findMethod(info.get(), name);
    if (!method) throw LangError("Undefined method: " + info->name + "::" + name);
    checkVisibility(info->name, method->visibility);
    if (!method->isStatic && self.type == ValueType::Object) {
        if (method->chunk) return executeChunk(*method->chunk, args, argc, self, selfClass);
        std::vector<Value> copied(args, args + argc);
        return callMethodFrom(info, self, name, copied);
    }
    if (method->chunk) return executeChunk(*method->chunk, args, argc, makeVoid(), "");
    std::vector<Value> copied(args, args + argc);
    return callStaticMethod(info, name, copied);
}

Value Interpreter::invokeNamespaceFunction(const std::shared_ptr<NamespaceInfo>& info,
                                           const std::string& name, const Value* args, int argc) {
    auto it = info->functions.find(name);
    if (it == info->functions.end()) {
        throw LangError("Undefined namespace function: " + info->name + "::" + name);
    }
    return invokeFunctionValue(it->second, args, argc, makeVoid(), "");
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
    if (resolved.type == ValueType::Object && resolved.objPtr() && !resolved.objPtr()->destructed &&
        seen.insert(resolved.objPtr().get()).second) {
        objects.push_back(resolved.objPtr());
    }
}

void Interpreter::shutdown() {
    for (auto& connection : mysqlConnections) {
        if (connection) mysqlClose(*connection);
    }
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

        for (const auto& value : vmGlobals) {
            collectLiveObjects(value, objects, seenObjects);
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
            self.payload = object;
            try {
                callMethod(self, "__destruct", {});
            } catch (...) {
            }
            for (auto& property : object->properties) {
                property.second = makeVoid();
            }
            object->owned.clear();
        }
    }
    shuttingDown = true;

    auto clearObjectValues = [](Value& value) {
        if (value.type == ValueType::Object || value.type == ValueType::ObjectWeak) {
            value = makeVoid();
        }
    };
    for (auto& value : vmGlobals) {
        clearObjectValues(value);
    }
    for (auto& entry : globals->values) {
        clearObjectValues(entry.second);
    }
    for (auto& entry : functions) {
        (void)entry;
    }
    for (auto& entry : classes) {
        if (entry.second) {
            for (auto& constant : entry.second->constants) clearObjectValues(constant.second);
            for (auto& property : entry.second->defaultProperties) clearObjectValues(property.second);
        }
    }
    for (auto& entry : namespaces) {
        if (entry.second) {
            for (auto& constant : entry.second->constants) clearObjectValues(constant.second);
        }
    }
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
    if (object.type != ValueType::Object || !object.objPtr()) {
        throw LangError("Attempt to call a method on a non-object");
    }
    return callMethodFrom(object.objPtr()->classInfo, object, name, args);
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
    if (object.type == ValueType::Object && object.objPtr() && object.objPtr()->classInfo) {
        actualClass = object.objPtr()->classInfo->name;
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

Value vmBinary(Op op, const Value& leftValue, const Value& rightValue) {
    switch (op) {
        case Op::ADD:
            if (leftValue.type == ValueType::String || rightValue.type == ValueType::String) {
                return makeString(valueToString(leftValue) + valueToString(rightValue));
            }
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                return makeInt(leftValue.ival + rightValue.ival);
            }
            return makeDouble(asNumber(leftValue) + asNumber(rightValue));
        case Op::SUB:
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                return makeInt(leftValue.ival - rightValue.ival);
            }
            return makeDouble(asNumber(leftValue) - asNumber(rightValue));
        case Op::MUL:
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                return makeInt(leftValue.ival * rightValue.ival);
            }
            return makeDouble(asNumber(leftValue) * asNumber(rightValue));
        case Op::DIV:
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                if (rightValue.ival == 0) throw LangError("Division by zero");
                return makeInt(leftValue.ival / rightValue.ival);
            }
            if (asNumber(rightValue) == 0.0) throw LangError("Division by zero");
            return makeDouble(asNumber(leftValue) / asNumber(rightValue));
        case Op::MOD:
            if (leftValue.type == ValueType::Int && rightValue.type == ValueType::Int) {
                if (rightValue.ival == 0) throw LangError("Division by zero");
                return makeInt(leftValue.ival % rightValue.ival);
            }
            if (asNumber(rightValue) == 0.0) throw LangError("Division by zero");
            return makeDouble(std::fmod(asNumber(leftValue), asNumber(rightValue)));
        case Op::CONCAT:
            return makeString(valueToString(leftValue) + valueToString(rightValue));
        case Op::EQ: return makeBool(valuesEqual(leftValue, rightValue));
        case Op::NE: return makeBool(!valuesEqual(leftValue, rightValue));
        case Op::GT: return makeBool(compareValues(leftValue, rightValue) > 0);
        case Op::GE: return makeBool(compareValues(leftValue, rightValue) >= 0);
        case Op::LT: return makeBool(compareValues(leftValue, rightValue) < 0);
        case Op::LE: return makeBool(compareValues(leftValue, rightValue) <= 0);
        default: throw LangError("Unsupported binary operator");
    }
}

int Interpreter::constantIndex(FunctionChunk& chunk, const Value& value) {
    chunk.constants.push_back(value);
    return static_cast<int>(chunk.constants.size()) - 1;
}

int Interpreter::nameIndex(FunctionChunk& chunk, const std::string& name) {
    chunk.names.push_back(name);
    return static_cast<int>(chunk.names.size()) - 1;
}

void Interpreter::compileProgram(const std::vector<StmtPtr>& program) {
    programChunk = ::compileProgram(program);
    globalSlots = programChunk->globalSlots;
    std::size_t count = static_cast<std::size_t>(programChunk->localCount);
    vmGlobals.resize(count);
    vmGlobalConst.assign(count, false);
    vmGlobalInferred.assign(count, ValueType::Void);
    programChunk->localCount = 0;
}

Value Interpreter::runProgram() {
    if (!programChunk) {
        return makeVoid();
    }
    return executeChunk(*programChunk, nullptr, 0, makeVoid(), "");
}

Value Interpreter::executeChunk(const FunctionChunk& chunk, const Value* args, int argc,
                                const Value& self, const std::string& selfClass) {
    Frame frame;
    frame.chunk = &chunk;
    if (argc != static_cast<int>(chunk.paramNames.size())) {
        throw LangError("Function expects " + std::to_string(chunk.paramNames.size()) +
                        " arguments but got " + std::to_string(argc));
    }
    if (chunk.localCount > 32) {
        throw LangError("Too many local variables in function");
    }
    for (int i = 0; i < argc && i < static_cast<int>(chunk.paramNames.size()); ++i) {
        frame.slots[i] = args[i];
    }
    frame.self = self;
    frame.selfClass = selfClass;
    return runFrame(frame);
}

Value Interpreter::runFrame(Frame& frame) {
    const FunctionChunk& chunk = *frame.chunk;
    Value stack[64];
    int stackPointer = 0;

    auto push = [&stack, &stackPointer](const Value& value) {
        stack[stackPointer++] = value;
    };
    auto pop = [&stack, &stackPointer]() {
        Value value = stack[--stackPointer];
        stack[stackPointer] = Value{};
        return value;
    };

    while (frame.ip < chunk.code.size()) {
        const Instruction& instruction = chunk.code[frame.ip++];

        switch (instruction.op) {
            case Op::CONST:
                push(chunk.constants[static_cast<std::size_t>(instruction.a)]);
                break;

            case Op::LOAD_SLOT:
                if (instruction.b == 1) push(frame.slots[static_cast<std::size_t>(instruction.a)]);
                else push(vmGlobals[static_cast<std::size_t>(instruction.a)]);
                break;

            case Op::STORE_SLOT: {
                Value value = pop();
                std::size_t slot = static_cast<std::size_t>(instruction.a);
                std::string varName = instruction.c >= 0
                    ? chunk.names[static_cast<std::size_t>(instruction.c)] : std::string("");
                if (instruction.b == 1) {
                    if (frame.constSlots[slot]) throw LangError("Cannot assign to constant: " + varName);
                    if (frame.inferred[slot] != ValueType::Void && frame.inferred[slot] != value.type) {
                        throw LangError("Type mismatch for variable: " + varName);
                    }
                    frame.slots[slot] = value;
                } else {
                    if (vmGlobalConst[slot]) throw LangError("Cannot assign to constant: " + varName);
                    if (vmGlobalInferred[slot] != ValueType::Void && vmGlobalInferred[slot] != value.type) {
                        throw LangError("Type mismatch for variable: " + varName);
                    }
                    vmGlobals[slot] = value;
                }
                break;
            }

            case Op::LOAD_NAME:
                push(resolveName(*this, chunk.names[static_cast<std::size_t>(instruction.a)]));
                break;

            case Op::ADD:
            case Op::SUB:
            case Op::MUL:
            case Op::DIV:
            case Op::MOD:
            case Op::CONCAT:
            case Op::EQ:
            case Op::NE:
            case Op::GT:
            case Op::GE:
            case Op::LT:
            case Op::LE: {
                Value right = pop();
                Value left = pop();
                if (left.type == ValueType::Int && right.type == ValueType::Int) {
                    switch (instruction.op) {
                        case Op::ADD: push(makeInt(left.ival + right.ival)); break;
                        case Op::SUB: push(makeInt(left.ival - right.ival)); break;
                        case Op::MUL: push(makeInt(left.ival * right.ival)); break;
                        case Op::DIV:
                            if (right.ival == 0) throw LangError("Division by zero");
                            push(makeInt(left.ival / right.ival));
                            break;
                        case Op::MOD:
                            if (right.ival == 0) throw LangError("Division by zero");
                            push(makeInt(left.ival % right.ival));
                            break;
                        case Op::EQ: push(makeBool(left.ival == right.ival)); break;
                        case Op::NE: push(makeBool(left.ival != right.ival)); break;
                        case Op::GT: push(makeBool(left.ival > right.ival)); break;
                        case Op::GE: push(makeBool(left.ival >= right.ival)); break;
                        case Op::LT: push(makeBool(left.ival < right.ival)); break;
                        case Op::LE: push(makeBool(left.ival <= right.ival)); break;
                        default: push(vmBinary(instruction.op, left, right)); break;
                    }
                } else {
                    push(vmBinary(instruction.op, left, right));
                }
                break;
            }

            case Op::AND:
            case Op::OR: {
                Value right = pop();
                Value left = pop();
                push(makeBool(instruction.op == Op::AND
                    ? (truthy(left) && truthy(right))
                    : (truthy(left) || truthy(right))));
                break;
            }

            case Op::NOT: push(makeBool(!truthy(pop()))); break;

            case Op::NEG: {
                Value value = pop();
                if (value.type == ValueType::Int) push(makeInt(-value.ival));
                else push(makeDouble(-asNumber(value)));
                break;
            }

            case Op::BOOL: push(makeBool(truthy(pop()))); break;

            case Op::JUMP:
                frame.ip = static_cast<std::size_t>(instruction.a);
                break;

            case Op::JUMP_IF_FALSE:
                if (!truthy(pop())) frame.ip = static_cast<std::size_t>(instruction.a);
                break;

            case Op::JUMP_IF_TRUE:
                if (truthy(pop())) frame.ip = static_cast<std::size_t>(instruction.a);
                break;

            case Op::POP: pop(); break;

            case Op::DUP: push(stack[stackPointer - 1]); break;

            case Op::CALL: {
                int argc = instruction.a;
                Value argBuffer[16];
                for (int i = argc - 1; i >= 0; --i) argBuffer[i] = pop();
                Value callee = pop();
                if (callee.type != ValueType::Function || !callee.fnPtr()) {
                    throw LangError("Attempt to call a non-function value");
                }
                push(invokeFunctionValue(*callee.fnPtr(), argBuffer, argc, makeVoid(), ""));
                break;
            }

            case Op::CALL_NATIVE: {
                const std::string& name = chunk.names[static_cast<std::size_t>(instruction.a)];
                int argc = instruction.b;
                Value argBuffer[16];
                for (int i = argc - 1; i >= 0; --i) argBuffer[i] = pop();
                push(callNative(name, argBuffer, argc));
                break;
            }

            case Op::CALL_METHOD: {
                const std::string& name = chunk.names[static_cast<std::size_t>(instruction.a)];
                int argc = instruction.b;
                Value argBuffer[16];
                for (int i = argc - 1; i >= 0; --i) argBuffer[i] = pop();
                Value object = resolveObject(pop());
                bool handled = false;
                Value special = threadMethod(object, name, argBuffer, argc, handled);
                if (!handled) {
                    special = httpMethod(object, name, argBuffer, argc, handled);
                }
                if (!handled) {
                    special = mysqlMethod(object, name, argBuffer, argc, handled);
                }
                if (!handled) {
                    special = collectionMethod(object, name, argBuffer, argc, handled);
                }
                if (handled) {
                    push(special);
                    break;
                }
                if (object.type != ValueType::Object || !object.objPtr()) {
                    throw LangError("Attempt to call a method on a non-object");
                }
                push(invokeMethod(object.objPtr()->classInfo, object, name, argBuffer, argc));
                break;
            }

            case Op::CALL_STATIC: {
                const std::string& name = chunk.names[static_cast<std::size_t>(instruction.a)];
                int argc = instruction.b;
                Value argBuffer[16];
                for (int i = argc - 1; i >= 0; --i) argBuffer[i] = pop();
                Value owner = pop();
                if (owner.type == ValueType::Namespace && owner.nsPtr()) {
                    push(invokeNamespaceFunction(owner.nsPtr(), name, argBuffer, argc));
                    break;
                }
                if (owner.type != ValueType::Class || !owner.clsPtr()) {
                    throw LangError("Attempt to call a static method on a non-class");
                }
                push(invokeStatic(owner.clsPtr(), name, argBuffer, argc, frame.self, frame.selfClass));
                break;
            }

            case Op::LOAD_MEMBER: {
                Value object = resolveObject(pop());
                if (object.type != ValueType::Object || !object.objPtr()) {
                    throw LangError("Attempt to access a member on a non-object");
                }
                const std::string& name = chunk.names[static_cast<std::size_t>(instruction.a)];
                const std::shared_ptr<ClassInfo>& info = object.objPtr()->classInfo;
                checkVisibility(info->name, findPropertyVisibility(info.get(), name));
                auto it = object.objPtr()->properties.find(name);
                if (it != object.objPtr()->properties.end()) {
                    push(it->second);
                } else {
                    const Value* fallback = findDefaultProperty(info.get(), name);
                    if (!fallback) throw LangError("Undefined property: " + info->name + "->" + name);
                    push(*fallback);
                }
                break;
            }

            case Op::STORE_MEMBER: {
                Value rawValue = pop();
                Value object = resolveObject(pop());
                if (object.type != ValueType::Object || !object.objPtr()) {
                    throw LangError("Attempt to set a member on a non-object");
                }
                Value value = storeObject(rawValue, object.objPtr().get());
                const std::string& name = chunk.names[static_cast<std::size_t>(instruction.a)];
                const std::shared_ptr<ClassInfo>& info = object.objPtr()->classInfo;
                checkVisibility(info->name, findPropertyVisibility(info.get(), name));
                auto it = object.objPtr()->properties.find(name);
                if (it == object.objPtr()->properties.end()) {
                    throw LangError("Undefined property: " + info->name + "->" + name);
                }
                it->second = value;
                push(value);
                break;
            }

            case Op::LOAD_STATIC: {
                Value owner = pop();
                const std::string& name = chunk.names[static_cast<std::size_t>(instruction.a)];
                if (owner.type == ValueType::Namespace && owner.nsPtr()) {
                    const std::shared_ptr<NamespaceInfo>& info = owner.nsPtr();
                    auto nested = info->namespaces.find(name);
                    if (nested != info->namespaces.end()) {
                        push(makeNamespaceValue(nested->second));
                        break;
                    }
                    auto classIt = info->classes.find(name);
                    if (classIt != info->classes.end()) {
                        Value value; value.type = ValueType::Class; value.payload = classIt->second;
                        push(value);
                        break;
                    }
                    auto functionIt = info->functions.find(name);
                    if (functionIt != info->functions.end()) {
                        push(makeFunctionValue(functionIt->second));
                        break;
                    }
                    auto constantIt = info->constants.find(name);
                    if (constantIt != info->constants.end()) {
                        push(constantIt->second);
                        break;
                    }
                    throw LangError("Undefined namespace member: " + info->name + "::" + name);
                }
                if (owner.type != ValueType::Class || !owner.clsPtr()) {
                    throw LangError("Attempt to access a static member on a non-class");
                }
                const Value* constant = findConstant(owner.clsPtr().get(), name);
                if (!constant) throw LangError("Undefined static member: " + owner.clsPtr()->name + "::" + name);
                push(*constant);
                break;
            }

            case Op::NEW: {
                const std::string& name = chunk.names[static_cast<std::size_t>(instruction.a)];
                int argc = instruction.b;
                Value argBuffer[16];
                for (int i = argc - 1; i >= 0; --i) argBuffer[i] = pop();
                std::shared_ptr<ClassInfo> info = findClassInfo(*this, name);
                if (!info) throw LangError("Undefined class: " + name);
                push(createObjectFrom(info, argBuffer, argc));
                break;
            }

            case Op::THIS:
                if (frame.self.type != ValueType::Object) {
                    throw LangError("this is only available inside a method");
                }
                push(frame.self);
                break;

            case Op::PARENT: {
                std::shared_ptr<ClassInfo> info = findClassInfo(*this, frame.selfClass);
                if (!info || !info->parent) throw LangError("parent is not available in this class");
                Value value; value.type = ValueType::Class; value.payload = info->parent;
                push(value);
                break;
            }

            case Op::MAKE_FUNCTION: {
                std::shared_ptr<FunctionChunk> child = chunk.children[static_cast<std::size_t>(instruction.a)];
                FunctionValue function;
                function.chunk = child;
                function.body = child->astBody;
                function.parameters = child->paramNames;
                function.isAsync = child->isAsync;
                push(makeFunctionValue(function));
                break;
            }

            case Op::AWAIT:
                push(awaitValue(pop()));
                break;

            case Op::DEFINE_FUNCTION: {
                const std::string& name = chunk.names[static_cast<std::size_t>(instruction.a)];
                Value value = pop();
                FunctionValue function;
                if (value.type == ValueType::Function && value.fnPtr()) function = *value.fnPtr();
                if (currentNamespace) currentNamespace->functions[name] = function;
                else functions[name] = function;
                break;
            }

            case Op::DEFINE_VAR: {
                std::size_t slot = static_cast<std::size_t>(instruction.a);
                int flags = instruction.b;
                const std::string& varName = chunk.names[static_cast<std::size_t>(instruction.c)];
                if ((flags & 4) != 0) {
                    if (frame.constSlots[slot]) throw LangError("Cannot redeclare constant: " + varName);
                } else if (vmGlobalConst[slot]) {
                    throw LangError("Cannot redeclare constant: " + varName);
                }
                Value value = pop();
                if ((flags & 4) != 0) {
                    frame.slots[slot] = value;
                    frame.constSlots[slot] = (flags & 1) != 0;
                    frame.inferred[slot] = (flags & 2) != 0 ? value.type : ValueType::Void;
                } else {
                    vmGlobals[slot] = value;
                    vmGlobalConst[slot] = (flags & 1) != 0;
                    vmGlobalInferred[slot] = (flags & 2) != 0 ? value.type : ValueType::Void;
                    if ((flags & 1) != 0 && currentNamespace) {
                        currentNamespace->constants[chunk.names[static_cast<std::size_t>(instruction.c)]] = value;
                    }
                }
                break;
            }

            case Op::STORE_STATIC: {
                Value value = pop();
                Value owner = pop();
                const std::string& name = chunk.names[static_cast<std::size_t>(instruction.a)];
                if (owner.type == ValueType::Class && owner.clsPtr()) {
                    owner.clsPtr()->constants[name] = value;
                    break;
                }
                if (owner.type == ValueType::Namespace && owner.nsPtr()) {
                    owner.nsPtr()->constants[name] = value;
                    break;
                }
                throw LangError("Attempt to set a static member on a non-class");
            }

            case Op::EXEC_NODE:
                chunk.statements[static_cast<std::size_t>(instruction.a)]->exec(*this);
                break;

            case Op::EVAL_NODE:
                push(chunk.expressions[static_cast<std::size_t>(instruction.a)]->eval(*this));
                break;

            case Op::RETURN:
                return pop();

            case Op::RETURN_VOID:
                return makeVoid();

            case Op::HALT:
                return makeVoid();
        }
    }
    return makeVoid();
}

std::shared_ptr<Interpreter> Interpreter::cloneForThread() const {
    auto child = std::make_shared<Interpreter>();
    child->programChunk = programChunk;
    child->vmGlobals = vmGlobals;
    child->vmGlobalConst = vmGlobalConst;
    child->vmGlobalInferred = vmGlobalInferred;
    child->functions = functions;
    child->classes = classes;
    child->namespaces = namespaces;
    child->usingNamespaces = usingNamespaces;
    child->currentNamespace = currentNamespace;
    child->globals->values = globals->values;
    child->globals->inferredTypes = globals->inferredTypes;
    child->globals->constants = globals->constants;

    auto detachThreadFutures = [](Value& value) {
        if (value.type != ValueType::Object || !value.objPtr()) {
            return;
        }
        if (value.objPtr()->classInfo->name != "Thread") {
            return;
        }
        auto copy = std::make_shared<ObjectInstance>();
        copy->classInfo = value.objPtr()->classInfo;
        copy->properties = value.objPtr()->properties;
        copy->properties["__future"] = makeVoid();
        copy->constructed = true;
        value.payload = copy;
    };

    for (auto& entry : child->globals->values) {
        detachThreadFutures(entry.second);
    }
    for (auto& value : child->vmGlobals) {
        detachThreadFutures(value);
    }
    return child;
}

Value Interpreter::spawnAsync(const FunctionValue& function, const Value* args, int argc) {
    auto state = std::make_shared<FutureState>();
    auto worker = cloneForThread();
    state->worker = worker;

    FunctionValue copiedFunction = function;
    std::vector<Value> copiedArgs(args, args + argc);

    FutureState* rawState = state.get();
    state->thread = std::thread([worker, copiedFunction, copiedArgs, rawState]() {
        try {
            Value result;
            if (copiedFunction.chunk) {
                result = worker->executeChunk(*copiedFunction.chunk, copiedArgs.data(),
                                              static_cast<int>(copiedArgs.size()), makeVoid(), "");
            } else {
                result = worker->callFunctionValue(copiedFunction, copiedArgs);
            }
            rawState->set(result);
        } catch (const std::exception& error) {
            Value failure;
            failure.type = ValueType::String;
            failure.setString(std::string("Async error: ") + error.what());
            rawState->set(failure);
        }
        worker->shutdown();
    });

    Value future;
    future.type = ValueType::Future;
    future.payload = state;
    return future;
}

Value Interpreter::awaitValue(const Value& value) {
    if (value.type == ValueType::Future) {
        std::shared_ptr<FutureState> state = value.futurePtr();
        return state ? state->get() : makeVoid();
    }
    return value;
}

Value Interpreter::getMember(const Value& object, const std::string& name) const {
    Value resolved = resolveObject(object);
    if (resolved.type != ValueType::Object || !resolved.objPtr()) {
        throw LangError("Attempt to access a member on a non-object");
    }
    auto it = resolved.objPtr()->properties.find(name);
    if (it != resolved.objPtr()->properties.end()) {
        return it->second;
    }
    const Value* fallback = findDefaultProperty(resolved.objPtr()->classInfo.get(), name);
    return fallback ? *fallback : makeVoid();
}

void Interpreter::setMember(const Value& object, const std::string& name, const Value& value) {
    Value resolved = resolveObject(object);
    if (resolved.type != ValueType::Object || !resolved.objPtr()) {
        throw LangError("Attempt to set a member on a non-object");
    }
    auto it = resolved.objPtr()->properties.find(name);
    if (it == resolved.objPtr()->properties.end()) {
        throw LangError("Undefined property: " + resolved.objPtr()->classInfo->name + "->" + name);
    }
    it->second = storeObject(value, resolved.objPtr().get());
}

Value Interpreter::makeThreadValue() {
    Value value;
    value.type = ValueType::Thread;
    value.payload = std::make_shared<ThreadState>();
    return value;
}

Value Interpreter::threadStart(const Value& object, const Value* args, int argc) {
    Value fn = getMember(object, "__fn");
    if (fn.type != ValueType::Function || !fn.fnPtr()) {
        throw LangError("Thread requires a function");
    }
    Value future = spawnAsync(*fn.fnPtr(), args, argc);
    setMember(object, "__future", future);
    return makeVoid();
}

Value Interpreter::threadJoin(const Value& object) {
    Value future = getMember(object, "__future");
    return awaitValue(future);
}

Value Interpreter::threadMethod(const Value& object, const std::string& name, const Value* args,
                                int argc, bool& handled) {
    Value resolved = resolveObject(object);
    if (resolved.type != ValueType::Object || !resolved.objPtr() ||
        resolved.objPtr()->classInfo->name != "Thread") {
        handled = false;
        return makeVoid();
    }
    handled = true;
    if (name == "start") {
        return threadStart(resolved, args, argc);
    }
    if (name == "join") {
        return threadJoin(resolved);
    }
    handled = false;
    return makeVoid();
}

namespace {

std::string lowercaseHeader(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

Value makeHttpValue(ValueType type, const std::shared_ptr<void>& payload) {
    Value value;
    value.type = type;
    value.payload = payload;
    return value;
}

}

void httpRunHandler(Interpreter& interpreter, const FunctionValue& handler,
                    HttpRequestData& request, HttpResponseData& response) {
    auto requestData = std::make_shared<HttpRequestData>(request);
    auto responseData = std::make_shared<HttpResponseData>();
    Value req = makeHttpValue(ValueType::HttpRequest, requestData);
    Value res = makeHttpValue(ValueType::HttpResponse, responseData);
    Value args[2] = {req, res};

    try {
        Value future = interpreter.spawnAsync(handler, args, 2);
        interpreter.awaitValue(future);
    } catch (const std::exception&) {
        responseData->status = 500;
        responseData->body = "Internal Server Error";
    }

    response.status = responseData->status;
    response.body = responseData->body;
    response.headers = responseData->headers;
    response.cookies = responseData->cookies;
    response.sendFilePath = responseData->sendFilePath;
}

Value Interpreter::httpMethod(const Value& object, const std::string& name, const Value* args,
                              int argc, bool& handled) {
    handled = true;

    if (object.type == ValueType::HttpServer) {
        auto state = object.serverPtr();
        if (!state) { handled = false; return makeVoid(); }
        std::string method;
        if (name == "get") method = "GET";
        else if (name == "post") method = "POST";
        else if (name == "put") method = "PUT";
        else if (name == "del" || name == "delete") method = "DELETE";
        else if (name == "all") method = "ALL";

        if (!method.empty()) {
            if (argc < 2 || args[0].type != ValueType::String ||
                args[1].type != ValueType::Function || !args[1].fnPtr()) {
                throw LangError("Route requires a path and a function");
            }
            std::lock_guard<std::mutex> lock(state->mutex);
            state->routes.push_back({method, args[0].str(), args[1].fnPtr()});
            return makeVoid();
        }

        if (name == "listen") {
            if (argc < 1) throw LangError("listen requires a port");
            state->port = static_cast<int>(args[0].ival);
            httpServe(*state);
            return makeVoid();
        }
        if (name == "port") {
            return makeInt(state->port);
        }
        if (name == "secure") {
            if (argc < 2) throw LangError("secure requires cert and key paths");
            state->secure = true;
            state->certFile = args[0].str();
            state->keyFile = args[1].str();
            return makeVoid();
        }
        if (name == "limit") {
            if (argc > 0) state->maxConnections = static_cast<int>(args[0].ival);
            return makeInt(state->maxConnections);
        }
        if (name == "connections") {
            return makeInt(state->activeConnections.load());
        }
        if (name == "served") {
            return makeInt(static_cast<long long>(state->servedRequests.load()));
        }
        if (name == "close") {
            state->running = false;
            if (state->listenFd >= 0) {
                ::shutdown(state->listenFd, SHUT_RDWR);
                ::close(state->listenFd);
            }
            return makeVoid();
        }
        handled = false;
        return makeVoid();
    }

    if (object.type == ValueType::HttpRequest) {
        auto req = object.requestPtr();
        if (!req) { handled = false; return makeVoid(); }
        if (name == "method") return makeString(req->method);
        if (name == "path") return makeString(req->path);
        if (name == "query") return makeString(req->query);
        if (name == "body") return makeString(req->body);
        if (name == "address") return makeString(req->remoteAddress);
        if (name == "header") {
            if (argc < 1) return makeString("");
            return makeString(httpHeaderValue(req->headers, args[0].str()));
        }
        if (name == "headers") {
            std::string result;
            for (const auto& header : req->headers) {
                if (!result.empty()) result += "; ";
                result += header.name + "=" + header.value;
            }
            return makeString(result);
        }
        if (name == "cookie") {
            if (argc < 1) return makeString("");
            return makeString(httpCookieValue(req->cookies, args[0].str()));
        }
        if (name == "cookies") {
            std::string result;
            for (const auto& cookie : req->cookies) {
                if (!result.empty()) result += "; ";
                result += cookie.first + "=" + cookie.second;
            }
            return makeString(result);
        }
        if (name == "form") {
            if (argc < 1) return makeString("");
            for (const auto& field : req->form) {
                if (field.first == args[0].str()) return makeString(field.second);
            }
            return makeString("");
        }
        if (name == "param") {
            if (argc < 1) return makeString("");
            std::string wanted = args[0].str();
            std::size_t start = 0;
            while (start <= req->query.size()) {
                std::size_t end = req->query.find('&', start);
                std::string part = req->query.substr(start, end == std::string::npos
                    ? std::string::npos : end - start);
                std::size_t equal = part.find('=');
                if (equal != std::string::npos && httpUrlDecode(part.substr(0, equal)) == wanted) {
                    return makeString(httpUrlDecode(part.substr(equal + 1)));
                }
                if (end == std::string::npos) break;
                start = end + 1;
            }
            for (const auto& field : req->form) {
                if (field.first == wanted) return makeString(field.second);
            }
            return makeString("");
        }
        if (name == "file") {
            if (argc < 1) return makeVoid();
            for (const auto& file : req->files) {
                if (file->field == args[0].str()) {
                    return makeHttpValue(ValueType::UploadedFile, file);
                }
            }
            return makeVoid();
        }
        if (name == "files") {
            std::string result;
            for (const auto& file : req->files) {
                if (!result.empty()) result += "; ";
                result += file->field + "=" + file->filename;
            }
            return makeString(result);
        }
        handled = false;
        return makeVoid();
    }

    if (object.type == ValueType::HttpResponse) {
        auto res = object.responsePtr();
        if (!res) { handled = false; return makeVoid(); }
        if (name == "status") {
            if (argc > 0) res->status = static_cast<int>(args[0].ival);
            return makeInt(res->status);
        }
        if (name == "send") {
            if (argc > 0) res->body = args[0].str();
            return makeVoid();
        }
        if (name == "json") {
            if (argc > 0) res->body = args[0].str();
            bool found = false;
            for (auto& header : res->headers) {
                if (header.name == "Content-Type") {
                    header.value = "application/json; charset=utf-8";
                    found = true;
                }
            }
            if (!found) res->headers.push_back({"Content-Type", "application/json; charset=utf-8"});
            return makeVoid();
        }
        if (name == "setHeader") {
            if (argc < 2) throw LangError("setHeader requires name and value");
            bool found = false;
            for (auto& header : res->headers) {
                if (lowercaseHeader(header.name) == lowercaseHeader(args[0].str())) {
                    header.value = args[1].str();
                    found = true;
                }
            }
            if (!found) res->headers.push_back({args[0].str(), args[1].str()});
            return makeVoid();
        }
        if (name == "getHeader") {
            if (argc < 1) return makeString("");
            for (const auto& header : res->headers) {
                if (lowercaseHeader(header.name) == lowercaseHeader(args[0].str())) {
                    return makeString(header.value);
                }
            }
            return makeString("");
        }
        if (name == "removeHeader") {
            if (argc < 1) return makeVoid();
            std::string wanted = lowercaseHeader(args[0].str());
            res->headers.erase(std::remove_if(res->headers.begin(), res->headers.end(),
                [&wanted](const HttpHeader& h) { return lowercaseHeader(h.name) == wanted; }),
                res->headers.end());
            return makeVoid();
        }
        if (name == "setCookie") {
            if (argc < 2) throw LangError("setCookie requires name and value");
            std::string cookie = args[0].str() + "=" + args[1].str() + "; Path=/";
            if (argc > 2) cookie += "; " + args[2].str();
            res->cookies.push_back(cookie);
            return makeVoid();
        }
        if (name == "getCookie") {
            if (argc < 1) return makeString("");
            for (const auto& cookie : res->cookies) {
                if (cookie.compare(0, args[0].str().size(), args[0].str()) == 0 &&
                    cookie.size() > args[0].str().size() && cookie[args[0].str().size()] == '=') {
                    std::size_t end = cookie.find(';');
                    return makeString(cookie.substr(args[0].str().size() + 1,
                        end == std::string::npos ? std::string::npos
                        : end - args[0].str().size() - 1));
                }
            }
            return makeString("");
        }
        if (name == "removeCookie") {
            if (argc < 1) return makeVoid();
            res->cookies.push_back(args[0].str() + "=; Path=/; Max-Age=0");
            return makeVoid();
        }
        if (name == "sendFile") {
            if (argc < 1) throw LangError("sendFile requires a path");
            res->sendFilePath = args[0].str();
            return makeVoid();
        }
        if (name == "redirect") {
            if (argc < 1) throw LangError("redirect requires a url");
            res->status = 302;
            bool found = false;
            for (auto& header : res->headers) {
                if (header.name == "Location") { header.value = args[0].str(); found = true; }
            }
            if (!found) res->headers.push_back({"Location", args[0].str()});
            return makeVoid();
        }
        handled = false;
        return makeVoid();
    }

    if (object.type == ValueType::UploadedFile) {
        auto file = object.uploadPtr();
        if (!file) { handled = false; return makeVoid(); }
        if (name == "field") return makeString(file->field);
        if (name == "filename") return makeString(file->filename);
        if (name == "contentType") return makeString(file->contentType);
        if (name == "size") return makeInt(static_cast<long long>(file->size()));
        if (name == "data") {
            return makeString(std::string(file->data.begin(), file->data.end()));
        }
        if (name == "save") {
            if (argc < 1) throw LangError("save requires a path");
            std::ofstream out(args[0].str(), std::ios::binary);
            if (!out) return makeBool(false);
            out.write(reinterpret_cast<const char*>(file->data.data()),
                      static_cast<std::streamsize>(file->data.size()));
            return makeBool(static_cast<bool>(out));
        }
        handled = false;
        return makeVoid();
    }

    handled = false;
    return makeVoid();
}

Value Interpreter::mysqlMethod(const Value& object, const std::string& name, const Value* args,
                               int argc, bool& handled) {
    handled = true;

    if (object.type == ValueType::MysqlConn) {
        auto connection = object.mysqlPtr();
        if (!connection) { handled = false; return makeVoid(); }

        if (name == "connect") {
            if (argc < 4) throw LangError("connect requires host, user, password and database");
            std::string host = args[0].str();
            std::string user = args[1].str();
            std::string password = args[2].str();
            std::string database = args[3].str();
            int port = argc > 4 ? static_cast<int>(args[4].ival) : 3306;
            auto connected = mysqlConnect(host, user, password, database, port);
            connection->handle = connected->handle;
            connection->ownerThread = connected->ownerThread;
            connection->lastError = connected->lastError;
            connected->handle = nullptr;
            return makeBool(connection->handle != nullptr);
        }
        if (name == "close") {
            mysqlClose(*connection);
            return makeVoid();
        }
        if (name == "query") {
            if (argc < 1) throw LangError("query requires a sql string");
            auto result = std::make_shared<MysqlResultData>();
            if (!mysqlExecute(*connection, args[0].str(), *result)) {
                return makeVoid();
            }
            Value value;
            value.type = ValueType::MysqlResult;
            value.payload = result;
            return value;
        }
        if (name == "escape") {
            if (argc < 1) return makeString("");
            return makeString(mysqlEscape(*connection, args[0].str()));
        }
        if (name == "lastError") {
            return makeString(mysqlLastError(*connection));
        }
        if (name == "insertId") {
            if (!connection->handle) return makeInt(0);
            return makeInt(static_cast<long long>(mysqlInsertId(*connection)));
        }
        if (name == "affectedRows") {
            if (!connection->handle) return makeInt(0);
            return makeInt(static_cast<long long>(mysqlAffectedRows(*connection)));
        }
        if (name == "ping") {
            if (!connection->handle) return makeBool(false);
            return makeBool(mysqlPing(*connection));
        }
        handled = false;
        return makeVoid();
    }

    if (object.type == ValueType::MysqlResult) {
        auto result = object.mysqlResultPtr();
        if (!result) { handled = false; return makeVoid(); }
        if (name == "rows") return makeInt(static_cast<long long>(result->rows.size()));
        if (name == "columns") {
            std::string joined;
            for (const auto& column : result->columns) {
                if (!joined.empty()) joined += ",";
                joined += column;
            }
            return makeString(joined);
        }
        if (name == "affectedRows") return makeInt(static_cast<long long>(result->affectedRows));
        if (name == "insertId") return makeInt(static_cast<long long>(result->insertId));
        if (name == "fetch") {
            if (result->rows.empty()) return makeVoid();
            Value row;
            row.type = ValueType::MysqlRow;
            row.payload = std::make_shared<MysqlResultRow>(result->rows.front());
            result->rows.erase(result->rows.begin());
            return row;
        }
        if (name == "each") {
            if (argc < 1 || args[0].type != ValueType::Function || !args[0].fnPtr()) {
                throw LangError("each requires a function");
            }
            for (const auto& source : result->rows) {
                Value row;
                row.type = ValueType::MysqlRow;
                row.payload = std::make_shared<MysqlResultRow>(source);
                Value callbackArgs[1] = {row};
                Value future = spawnAsync(*args[0].fnPtr(), callbackArgs, 1);
                awaitValue(future);
            }
            return makeVoid();
        }
        handled = false;
        return makeVoid();
    }

    if (object.type == ValueType::MysqlRow) {
        auto row = object.mysqlRowPtr();
        if (!row) { handled = false; return makeVoid(); }
        if (name == "get") {
            if (argc < 1) return makeString("");
            if (args[0].type == ValueType::String) {
                const std::string& wanted = args[0].str();
                for (std::size_t i = 0; i < row->columns.size(); ++i) {
                    if (row->columns[i] == wanted) {
                        return makeString(i < row->values.size() ? row->values[i] : "");
                    }
                }
                return makeString("");
            }
            long long index = args[0].ival;
            if (index < 0 || static_cast<std::size_t>(index) >= row->values.size()) return makeString("");
            return makeString(row->values[static_cast<std::size_t>(index)]);
        }
        if (name == "size") return makeInt(static_cast<long long>(row->values.size()));
        handled = false;
        return makeVoid();
    }

    handled = false;
    return makeVoid();
}

Value ArrayLiteralExpr::eval(Interpreter& interpreter) {
    auto array = std::make_shared<ArrayData>();
    for (const ExprPtr& element : elements) {
        array->items.push_back(element->eval(interpreter));
    }
    Value value;
    value.type = ValueType::Array;
    value.payload = array;
    return value;
}

Value MapLiteralExpr::eval(Interpreter& interpreter) {
    auto map = std::make_shared<MapData>();
    for (const auto& entry : entries) {
        Value key = entry.first->eval(interpreter);
        map->items[valueToString(key)] = entry.second->eval(interpreter);
    }
    Value value;
    value.type = ValueType::Map;
    value.payload = map;
    return value;
}

Value IndexExpr::eval(Interpreter& interpreter) {
    Value targetValue = object->eval(interpreter);
    Value keyValue = index->eval(interpreter);
    if (targetValue.type == ValueType::Array) {
        long long position = keyValue.type == ValueType::Int ? keyValue.ival
            : static_cast<long long>(asNumber(keyValue));
        const auto& items = targetValue.arrayPtr()->items;
        if (position < 0 || static_cast<std::size_t>(position) >= items.size()) return makeVoid();
        return items[static_cast<std::size_t>(position)];
    }
    if (targetValue.type == ValueType::Map) {
        auto it = targetValue.mapPtr()->items.find(valueToString(keyValue));
        return it == targetValue.mapPtr()->items.end() ? makeVoid() : it->second;
    }
    if (targetValue.type == ValueType::String) {
        long long position = keyValue.type == ValueType::Int ? keyValue.ival
            : static_cast<long long>(asNumber(keyValue));
        const std::string& text = targetValue.str();
        if (position < 0 || static_cast<std::size_t>(position) >= text.size()) return makeString("");
        return makeString(std::string(1, text[static_cast<std::size_t>(position)]));
    }
    throw LangError("Value is not indexable");
}

void IndexExpr::assign(Interpreter& interpreter, const Value& value) {
    Value targetValue = object->eval(interpreter);
    Value keyValue = index->eval(interpreter);
    if (targetValue.type == ValueType::Array) {
        long long position = keyValue.type == ValueType::Int ? keyValue.ival
            : static_cast<long long>(asNumber(keyValue));
        auto& items = targetValue.arrayPtr()->items;
        if (position < 0 || static_cast<std::size_t>(position) >= items.size()) {
            throw LangError("Array index out of range");
        }
        items[static_cast<std::size_t>(position)] = value;
        return;
    }
    if (targetValue.type == ValueType::Map) {
        targetValue.mapPtr()->items[valueToString(keyValue)] = value;
        return;
    }
    throw LangError("Value does not support index assignment");
}

ExecResult ForEachStmt::exec(Interpreter& interpreter) {
    Value source = iterable->eval(interpreter);
    std::vector<std::pair<Value, Value>> pairs;

    if (source.type == ValueType::Array) {
        long long index = 0;
        for (const Value& item : source.arrayPtr()->items) {
            pairs.emplace_back(makeInt(index++), item);
        }
    } else if (source.type == ValueType::Map) {
        for (const auto& entry : source.mapPtr()->items) {
            pairs.emplace_back(makeString(entry.first), entry.second);
        }
    } else if (source.type == ValueType::String) {
        long long index = 0;
        for (char character : source.str()) {
            pairs.emplace_back(makeInt(index++), makeString(std::string(1, character)));
        }
    } else {
        throw LangError("for-each requires an array, map or string");
    }

    for (const auto& pair : pairs) {
        auto scope = std::make_shared<Environment>(interpreter.environment);
        if (!valueName.empty()) scope->define(valueName, pair.second);
        if (!keyName.empty()) scope->define(keyName, pair.first);
        ExecResult result = interpreter.executeBlock(body ? body->statements : std::vector<StmtPtr>{}, scope);
        if (result.flow == Flow::Return) return result;
        if (result.flow == Flow::Break) break;
    }
    return ExecResult{};
}

ExecResult SwitchStmt::exec(Interpreter& interpreter) {
    Value target = value->eval(interpreter);
    std::size_t start = cases.size();
    for (std::size_t i = 0; i < cases.size(); ++i) {
        Value candidate = cases[i].value->eval(interpreter);
        if (valuesEqual(target, candidate)) { start = i; break; }
    }
    if (start == cases.size()) {
        for (const StmtPtr& statement : defaultBranch) {
            ExecResult result = statement->exec(interpreter);
            if (result.flow == Flow::Break) return ExecResult{};
            if (result.flow != Flow::Normal) return result;
        }
        return ExecResult{};
    }
    for (std::size_t i = start; i < cases.size(); ++i) {
        for (const StmtPtr& statement : cases[i].statements) {
            ExecResult result = statement->exec(interpreter);
            if (result.flow == Flow::Break) return ExecResult{};
            if (result.flow != Flow::Normal) return result;
        }
    }
    for (const StmtPtr& statement : defaultBranch) {
        ExecResult result = statement->exec(interpreter);
        if (result.flow == Flow::Break) return ExecResult{};
        if (result.flow != Flow::Normal) return result;
    }
    return ExecResult{};
}

ExecResult ThrowStmt::exec(Interpreter& interpreter) {
    ThrownException thrown;
    thrown.value = value ? value->eval(interpreter) : makeVoid();
    throw thrown;
}

ExecResult TryStmt::exec(Interpreter& interpreter) {
    ExecResult result;
    try {
        result = tryBlock->exec(interpreter);
    } catch (const ThrownException& thrown) {
        if (catchBlock && !catchName.empty()) {
            auto scope = std::make_shared<Environment>(interpreter.environment);
            scope->define(catchName, thrown.value);
            result = interpreter.executeBlock(catchBlock->statements, scope);
        } else {
            result.flow = Flow::Return;
            result.value = thrown.value;
        }
    }
    if (finallyBlock) {
        ExecResult finallyResult = finallyBlock->exec(interpreter);
        if (finallyResult.flow != Flow::Normal) return finallyResult;
    }
    return result;
}

Value Interpreter::collectionMethod(const Value& object, const std::string& name, const Value* args,
                                    int argc, bool& handled) {
    handled = true;

    if (object.type == ValueType::Array) {
        auto array = object.arrayPtr();
        if (name == "size" || name == "len") return makeInt(static_cast<long long>(array->items.size()));
        if (name == "push") {
            if (argc > 0) array->items.push_back(args[0]);
            return makeInt(static_cast<long long>(array->items.size()));
        }
        if (name == "get" || name == "at") {
            if (argc < 1 || args[0].ival < 0 || static_cast<std::size_t>(args[0].ival) >= array->items.size()) {
                return makeVoid();
            }
            return array->items[static_cast<std::size_t>(args[0].ival)];
        }
        if (name == "set") {
            if (argc < 2 || args[0].ival < 0 || static_cast<std::size_t>(args[0].ival) >= array->items.size()) {
                return makeBool(false);
            }
            array->items[static_cast<std::size_t>(args[0].ival)] = args[1];
            return makeBool(true);
        }
        if (name == "join") {
            std::string separator = argc > 0 ? valueToString(args[0]) : "";
            std::string result;
            for (const auto& item : array->items) {
                if (!result.empty()) result += separator;
                result += valueToString(item);
            }
            return makeString(result);
        }
        handled = false;
        return makeVoid();
    }

    if (object.type == ValueType::Map) {
        auto map = object.mapPtr();
        if (name == "size" || name == "len") return makeInt(static_cast<long long>(map->items.size()));
        if (name == "get") {
            if (argc < 1) return makeVoid();
            auto it = map->items.find(valueToString(args[0]));
            return it == map->items.end() ? makeVoid() : it->second;
        }
        if (name == "set") {
            if (argc < 2) return makeBool(false);
            map->items[valueToString(args[0])] = args[1];
            return makeBool(true);
        }
        if (name == "has") {
            if (argc < 1) return makeBool(false);
            return makeBool(map->items.count(valueToString(args[0])) != 0);
        }
        if (name == "remove") {
            if (argc < 1) return makeBool(false);
            return makeBool(map->items.erase(valueToString(args[0])) > 0);
        }
        if (name == "keys" || name == "values") {
            auto array = std::make_shared<ArrayData>();
            for (const auto& entry : map->items) {
                array->items.push_back(name == "keys" ? makeString(entry.first) : entry.second);
            }
            Value value;
            value.type = ValueType::Array;
            value.payload = array;
            return value;
        }
        handled = false;
        return makeVoid();
    }

    if (object.type == ValueType::Exception) {
        auto error = object.exceptionPtr();
        if (name == "message") return makeString(error->message);
        if (name == "type") return makeString(error->type);
        handled = false;
        return makeVoid();
    }

    handled = false;
    return makeVoid();
}

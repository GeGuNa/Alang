#pragma once

#include <array>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.h"
#include "http.h"
#include "mysql.h"
#include "token.hpp"
#include "vm.h"

struct Interpreter;

std::string valueToString(const Value& value);

struct Environment {
    std::unordered_map<std::string, Value> values;
    std::unordered_map<std::string, ValueType> inferredTypes;
    std::unordered_set<std::string> constants;
    std::shared_ptr<Environment> parent;
    std::weak_ptr<Environment> globalParent;
    bool isGlobal = false;
    mutable std::array<std::string, 4> cacheKeys;
    mutable std::array<Value*, 4> cacheValues{};
    mutable unsigned cacheCursor = 0;

    Value* cachedValue(const std::string& name) const;
    void rememberValue(const std::string& name, Value* value) const;

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
    bool isAsync = false;
    std::shared_ptr<FunctionChunk> chunk;
};

struct ClassInfo {
    std::string name;
    bool isStruct = false;
    std::shared_ptr<ClassInfo> parent;
    std::unordered_map<std::string, Value> defaultProperties;
    std::unordered_map<std::string, std::string> propertyVisibility;
    std::unordered_map<std::string, Value> constants;
    std::unordered_map<std::string, FunctionValue> methods;
};

struct ObjectInstance {
    std::shared_ptr<ClassInfo> classInfo;
    std::unordered_map<std::string, Value> properties;
    std::vector<std::shared_ptr<ObjectInstance>> owned;
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

struct ThrownException {
    Value value;
};

struct ArrayData {
    std::vector<Value> items;
};

struct MapData {
    std::map<std::string, Value> items;
};

struct ExceptionData {
    std::string message;
    std::string type = "Exception";
};

struct FutureState {
    std::shared_ptr<Interpreter> worker;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    bool ready = false;
    Value result;

    ~FutureState() {
        if (thread.joinable()) {
            thread.join();
        }
    }

    void set(Value value) {
        std::lock_guard<std::mutex> lock(mutex);
        result = std::move(value);
        ready = true;
        cv.notify_all();
    }

    Value get() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return ready; });
        return result;
    }
};

struct ThreadState {
    std::shared_ptr<FutureState> future;
    std::thread thread;
    bool started = false;

    void start(std::shared_ptr<FutureState> f) {
        if (started) return;
        started = true;
        future = std::move(f);
    }

    ~ThreadState() {
        if (thread.joinable()) {
            thread.join();
        }
    }
};

struct Frame {
    const FunctionChunk* chunk = nullptr;
    Value slots[32];
    bool constSlots[32] = {};
    ValueType inferred[32] = {};
    std::size_t ip = 0;
    Value self;
    std::string selfClass;
};

using NativeFn = std::function<Value(Interpreter&, const std::vector<Value>&)>;

struct Interpreter : std::enable_shared_from_this<Interpreter> {
    std::shared_ptr<Environment> globals = std::make_shared<Environment>();
    std::shared_ptr<Environment> environment = globals;
    std::unordered_map<std::string, FunctionValue> functions;
    std::unordered_map<std::string, NativeFn> natives;
    std::unordered_map<std::string, Value> nativeValues;
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

    std::shared_ptr<FunctionChunk> programChunk;
    std::vector<Value> vmGlobals;
    std::vector<unsigned char> vmGlobalConst;
    std::vector<ValueType> vmGlobalInferred;
    std::vector<Frame> frames;
    std::vector<std::shared_ptr<HttpServerState>> serverStates;
    std::vector<std::shared_ptr<MysqlConnectionData>> mysqlConnections;
    std::unordered_map<std::string, int> globalSlots;

    void compileProgram(const std::vector<StmtPtr>& program);
    Value runProgram();
    Value executeChunk(const FunctionChunk& chunk, const Value* args, int argc,
                       const Value& self, const std::string& selfClass);
    Value runFrame(Frame& frame);
    int constantIndex(FunctionChunk& chunk, const Value& value);
    int nameIndex(FunctionChunk& chunk, const std::string& name);

    Value callNative(const std::string& name, const Value* args, int argc);
    Value invokeFunctionValue(const FunctionValue& function, const Value* args, int argc,
                              const Value& self, const std::string& selfClass);
    Value invokeMethod(const std::shared_ptr<ClassInfo>& info, const Value& object,
                       const std::string& name, const Value* args, int argc);
    Value invokeStatic(const std::shared_ptr<ClassInfo>& info, const std::string& name,
                       const Value* args, int argc, const Value& self, const std::string& selfClass);
    Value invokeNamespaceFunction(const std::shared_ptr<NamespaceInfo>& info, const std::string& name,
                                  const Value* args, int argc);
    Value createObjectFrom(const std::shared_ptr<ClassInfo>& info, const Value* args, int argc);

    std::shared_ptr<Interpreter> cloneForThread() const;
    Value spawnAsync(const FunctionValue& function, const Value* args, int argc);
    Value awaitValue(const Value& value);
    Value getMember(const Value& object, const std::string& name) const;
    void setMember(const Value& object, const std::string& name, const Value& value);
    Value makeThreadValue();
    Value threadStart(const Value& object, const Value* args, int argc);
    Value threadJoin(const Value& object);
    Value threadMethod(const Value& object, const std::string& name, const Value* args,
                       int argc, bool& handled);
    Value httpMethod(const Value& object, const std::string& name, const Value* args,
                     int argc, bool& handled);
    Value mysqlMethod(const Value& object, const std::string& name, const Value* args,
                      int argc, bool& handled);
    Value collectionMethod(const Value& object, const std::string& name, const Value* args,
                           int argc, bool& handled);
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

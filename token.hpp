#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

enum TokType {
    T_EOF,
    T_IDENT,
    T_INT_LIT,
    T_FLOAT_LIT,
    T_STRING_LIT,
    T_CHAR_LIT,
    T_BOOL_LIT,
    T_FN,
    T_CLASS,
    T_INIT,
    T_DEINIT,
    T_THIS,
    T_NEW,
    T_NULL,
    T_IF,
    T_ELSE,
    T_FOR,
    T_WHILE,
    T_SWITCH,
    T_CASE,
    T_DEFAULT,
    T_BREAK,
    T_CONTINUE,
    T_RETURN,
    T_CONST,
    T_VAR,
    T_FUNCTION,
    T_THREAD,
    T_ASYNC,
    T_AWAIT,
    T_EXTENDS,
    T_EXTEND,
    T_NAMESPACE,
    T_USING,
    T_PARENT,
    T_PUBLIC,
    T_PRIVATE,
    T_PROTECTED,
    T_STATIC,
    T_VOID,
    T_BOOL,
    T_CHAR_TYPE,
    T_INT_TYPE,
    T_FLOAT_TYPE,
    T_DOUBLE_TYPE,
    T_STRING_TYPE,
    T_STR_TYPE,
    T_ASSIGN,
    T_PLUS,
    T_MINUS,
    T_STAR,
    T_SLASH,
    T_PERCENT,
    T_EQ,
    T_NE,
    T_GT,
    T_GE,
    T_LT,
    T_LE,
    T_NOT,
    T_AND,
    T_OR,
    T_SEMICOLON,
    T_COMMA,
    T_LPAREN,
    T_RPAREN,
    T_LBRACE,
    T_RBRACE,
    T_DOT,
    T_COLON,
    T_ARROW,
    T_DOUBLE_COLON,
    T_ERROR
};

struct Token {
    TokType type = T_EOF;
    std::string text;
    int line = 1;
    long long ival = 0;
    double dval = 0.0;
    bool bval = false;
    bool dollar = false;
};

enum class ValueType { Void, Null, Int, Double, Bool, String, Function, Object, ObjectWeak, Class, Namespace, Future, Thread, HttpServer, HttpRequest, HttpResponse, UploadedFile, MysqlConn, MysqlResult, MysqlRow };

struct FunctionValue;
struct ObjectInstance;
struct ClassInfo;
struct NamespaceInfo;
struct FutureState;
struct ThreadState;
struct HttpServerState;
struct HttpRequestData;
struct HttpResponseData;
struct UploadedFile;
struct MysqlConnectionData;
struct MysqlResultData;
struct MysqlResultRow;

struct WeakObjectRef {
    std::weak_ptr<ObjectInstance> ref;
};

struct Value {
    ValueType type = ValueType::Void;
    long long ival = 0;
    double dval = 0.0;
    bool bval = false;
    std::shared_ptr<void> payload;

    std::shared_ptr<WeakObjectRef> weakRef() const {
        return std::static_pointer_cast<WeakObjectRef>(payload);
    }

    std::shared_ptr<std::string> strPtr() const {
        return std::static_pointer_cast<std::string>(payload);
    }
    std::shared_ptr<FunctionValue> fnPtr() const {
        return std::static_pointer_cast<FunctionValue>(payload);
    }
    std::shared_ptr<ObjectInstance> objPtr() const {
        return std::static_pointer_cast<ObjectInstance>(payload);
    }
    std::shared_ptr<ClassInfo> clsPtr() const {
        return std::static_pointer_cast<ClassInfo>(payload);
    }
    std::shared_ptr<NamespaceInfo> nsPtr() const {
        return std::static_pointer_cast<NamespaceInfo>(payload);
    }
    std::shared_ptr<FutureState> futurePtr() const {
        return std::static_pointer_cast<FutureState>(payload);
    }
    std::shared_ptr<ThreadState> threadPtr() const {
        return std::static_pointer_cast<ThreadState>(payload);
    }
    std::shared_ptr<HttpServerState> serverPtr() const {
        return std::static_pointer_cast<HttpServerState>(payload);
    }
    std::shared_ptr<HttpRequestData> requestPtr() const {
        return std::static_pointer_cast<HttpRequestData>(payload);
    }
    std::shared_ptr<HttpResponseData> responsePtr() const {
        return std::static_pointer_cast<HttpResponseData>(payload);
    }
    std::shared_ptr<UploadedFile> uploadPtr() const {
        return std::static_pointer_cast<UploadedFile>(payload);
    }
    std::shared_ptr<MysqlConnectionData> mysqlPtr() const {
        return std::static_pointer_cast<MysqlConnectionData>(payload);
    }
    std::shared_ptr<MysqlResultData> mysqlResultPtr() const {
        return std::static_pointer_cast<MysqlResultData>(payload);
    }
    std::shared_ptr<MysqlResultRow> mysqlRowPtr() const {
        return std::static_pointer_cast<MysqlResultRow>(payload);
    }

    std::string& str() { return *strPtr(); }
    const std::string& str() const { return *strPtr(); }
    FunctionValue& fn() { return *fnPtr(); }
    ObjectInstance& obj() { return *objPtr(); }
    ClassInfo& cls() { return *clsPtr(); }
    NamespaceInfo& ns() { return *nsPtr(); }

    void setString(std::string value) {
        payload = std::make_shared<std::string>(std::move(value));
    }
};

inline Value makeVoid() {
    return Value{};
}

inline Value makeNull() {
    Value result;
    result.type = ValueType::Null;
    return result;
}

inline Value makeInt(long long value) {
    Value result;
    result.type = ValueType::Int;
    result.ival = value;
    return result;
}

inline Value makeDouble(double value) {
    Value result;
    result.type = ValueType::Double;
    result.dval = value;
    return result;
}

inline Value makeBool(bool value) {
    Value result;
    result.type = ValueType::Bool;
    result.bval = value;
    return result;
}

inline Value makeString(std::string value) {
    Value result;
    result.type = ValueType::String;
    result.setString(std::move(value));
    return result;
}

struct LangError : std::runtime_error {
    explicit LangError(const std::string& message) : std::runtime_error(message) {}
};

inline std::string lineMessage(const Token& token, const std::string& message) {
    return "line " + std::to_string(token.line) + ": " + message;
}

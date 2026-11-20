#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct FunctionValue;
struct Interpreter;
struct Value;
struct ssl_ctx_st;
struct ssl_st;

using HttpHandlerHook = void (*)(Interpreter&, const FunctionValue&, struct HttpRequestData&, struct HttpResponseData&);
void httpSetHandlerHook(HttpHandlerHook hook);

struct HttpHeader {
    std::string name;
    std::string value;
};

struct UploadedFile {
    std::string field;
    std::string filename;
    std::string contentType;
    std::vector<unsigned char> data;

    std::size_t size() const { return data.size(); }
};

struct HttpRequestData {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::string remoteAddress;
    std::vector<HttpHeader> headers;
    std::vector<std::pair<std::string, std::string>> cookies;
    std::vector<std::pair<std::string, std::string>> form;
    std::vector<std::shared_ptr<UploadedFile>> files;
};

struct HttpResponseData {
    int status = 200;
    std::string body;
    std::vector<HttpHeader> headers;
    std::vector<std::string> cookies;
    std::string sendFilePath;
    std::mutex mutex;
};

struct HttpRoute {
    std::string method;
    std::string pattern;
    std::shared_ptr<FunctionValue> handler;
};

struct HttpConnection {
    int fd = -1;
    ssl_st* ssl = nullptr;
};

struct HttpServerState {
    int port = 0;
    int listenFd = -1;
    bool secure = false;
    std::string certFile;
    std::string keyFile;
    int maxConnections = 0;
    std::atomic<int> activeConnections{0};
    std::atomic<long long> servedRequests{0};
    std::vector<HttpRoute> routes;
    std::vector<std::thread> workers;
    std::atomic<bool> running{false};
    std::mutex mutex;
    std::weak_ptr<Interpreter> owner;
};

std::string httpHeaderValue(const std::vector<HttpHeader>& headers, const std::string& name);
std::string httpCookieValue(const std::vector<std::pair<std::string, std::string>>& cookies,
                            const std::string& name);
std::string httpUrlDecode(const std::string& value);
bool httpParseRequest(HttpConnection& connection, HttpRequestData& request);
void httpWriteResponse(HttpConnection& connection, const HttpResponseData& response);
void httpServe(HttpServerState& state);

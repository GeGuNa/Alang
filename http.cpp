#include "http.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

namespace {

HttpHandlerHook g_handlerHook = nullptr;

std::string trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::size_t connRead(HttpConnection& connection, char* buffer, std::size_t count) {
    if (connection.ssl) {
        int n = SSL_read(connection.ssl, buffer, static_cast<int>(count));
        return n > 0 ? static_cast<std::size_t>(n) : 0;
    }
    ssize_t n = recv(connection.fd, buffer, count, 0);
    return n > 0 ? static_cast<std::size_t>(n) : 0;
}

void connWrite(HttpConnection& connection, const char* data, std::size_t count) {
    std::size_t sent = 0;
    while (sent < count) {
        std::size_t n = 0;
        if (connection.ssl) {
            int written = SSL_write(connection.ssl, data + sent, static_cast<int>(count - sent));
            n = written > 0 ? static_cast<std::size_t>(written) : 0;
        } else {
            ssize_t written = send(connection.fd, data + sent, count - sent, 0);
            n = written > 0 ? static_cast<std::size_t>(written) : 0;
        }
        if (n == 0) break;
        sent += n;
    }
}

std::string readExact(HttpConnection& connection, std::size_t count) {
    std::string result;
    result.resize(count);
    std::size_t total = 0;
    while (total < count) {
        std::size_t n = connRead(connection, result.data() + total, count - total);
        if (n == 0) break;
        total += n;
    }
    result.resize(total);
    return result;
}

std::string readUntil(HttpConnection& connection, const std::string& marker) {
    std::string result;
    char buffer[4096];
    while (result.find(marker) == std::string::npos) {
        std::size_t n = connRead(connection, buffer, sizeof(buffer));
        if (n == 0) break;
        result.append(buffer, n);
        if (result.size() > 16 * 1024 * 1024) break;
    }
    return result;
}

void parseCookies(HttpRequestData& request, const std::string& cookieHeader) {
    std::size_t start = 0;
    while (start <= cookieHeader.size()) {
        std::size_t end = cookieHeader.find(';', start);
        std::string part = cookieHeader.substr(start, end == std::string::npos
            ? std::string::npos : end - start);
        std::size_t equal = part.find('=');
        if (equal != std::string::npos) {
            request.cookies.emplace_back(trim(part.substr(0, equal)), trim(part.substr(equal + 1)));
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
}

void parseUrlEncoded(HttpRequestData& request, const std::string& body) {
    std::size_t start = 0;
    while (start <= body.size()) {
        std::size_t end = body.find('&', start);
        std::string part = body.substr(start, end == std::string::npos
            ? std::string::npos : end - start);
        std::size_t equal = part.find('=');
        if (equal != std::string::npos) {
            request.form.emplace_back(httpUrlDecode(part.substr(0, equal)),
                                      httpUrlDecode(part.substr(equal + 1)));
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
}

void parseMultipart(HttpRequestData& request, const std::string& body, const std::string& contentType) {
    std::size_t pos = contentType.find("boundary=");
    if (pos == std::string::npos) return;
    std::string boundary = contentType.substr(pos + 9);
    if (!boundary.empty() && boundary.front() == '"' && boundary.back() == '"') {
        boundary = boundary.substr(1, boundary.size() - 2);
    }
    std::string delimiter = "--" + boundary;

    std::size_t cursor = body.find(delimiter);
    if (cursor == std::string::npos) return;
    cursor += delimiter.size();

    while (cursor < body.size()) {
        if (body.compare(cursor, 2, "--") == 0) break;
        if (body.compare(cursor, 2, "\r\n") == 0) cursor += 2;
        std::size_t headerEnd = body.find("\r\n\r\n", cursor);
        if (headerEnd == std::string::npos) break;
        std::string partHeaders = body.substr(cursor, headerEnd - cursor);
        cursor = headerEnd + 4;

        std::string disposition;
        std::string partContentType;
        std::istringstream lines(partHeaders);
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string name = lowercase(trim(line.substr(0, colon)));
            std::string value = trim(line.substr(colon + 1));
            if (name == "content-disposition") disposition = value;
            if (name == "content-type") partContentType = value;
        }

        std::size_t next = body.find(delimiter, cursor);
        if (next == std::string::npos) break;
        std::size_t dataEnd = next;
        if (dataEnd >= 2 && body.compare(dataEnd - 2, 2, "\r\n") == 0) dataEnd -= 2;
        std::string data = body.substr(cursor, dataEnd - cursor);
        cursor = next + delimiter.size();

        std::string field;
        std::string filename;
        std::size_t namePos = disposition.find("name=\"");
        if (namePos != std::string::npos) {
            namePos += 6;
            std::size_t nameEnd = disposition.find('"', namePos);
            if (nameEnd != std::string::npos) field = disposition.substr(namePos, nameEnd - namePos);
        }
        std::size_t filePos = disposition.find("filename=\"");
        if (filePos != std::string::npos) {
            filePos += 10;
            std::size_t fileEnd = disposition.find('"', filePos);
            if (fileEnd != std::string::npos) filename = disposition.substr(filePos, fileEnd - filePos);
        }

        if (!filename.empty()) {
            auto file = std::make_shared<UploadedFile>();
            file->field = field;
            file->filename = filename;
            file->contentType = partContentType;
            file->data.assign(data.begin(), data.end());
            request.files.push_back(file);
        } else if (!field.empty()) {
            request.form.emplace_back(field, data);
        }
    }
}

std::string statusText(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

}

void httpSetHandlerHook(HttpHandlerHook hook) {
    g_handlerHook = hook;
}

std::string httpUrlDecode(const std::string& value) {
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            char hex[3] = {value[i + 1], value[i + 2], '\0'};
            char* end = nullptr;
            long code = std::strtol(hex, &end, 16);
            if (end && *end == '\0') {
                result.push_back(static_cast<char>(code));
                i += 2;
                continue;
            }
        }
        if (value[i] == '+') result.push_back(' ');
        else result.push_back(value[i]);
    }
    return result;
}

std::string httpHeaderValue(const std::vector<HttpHeader>& headers, const std::string& name) {
    std::string wanted = lowercase(name);
    for (const auto& header : headers) {
        if (lowercase(header.name) == wanted) return header.value;
    }
    return "";
}

std::string httpCookieValue(const std::vector<std::pair<std::string, std::string>>& cookies,
                            const std::string& name) {
    for (const auto& cookie : cookies) {
        if (cookie.first == name) return cookie.second;
    }
    return "";
}

bool httpParseRequest(HttpConnection& connection, HttpRequestData& request) {
    std::string head = readUntil(connection, "\r\n\r\n");
    if (head.empty()) return false;
    std::size_t headerEnd = head.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return false;
    std::string headerPart = head.substr(0, headerEnd);
    std::string rest = head.substr(headerEnd + 4);

    std::istringstream lines(headerPart);
    std::string requestLine;
    if (!std::getline(lines, requestLine)) return false;
    if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();
    std::istringstream parts(requestLine);
    parts >> request.method >> request.path;

    std::size_t question = request.path.find('?');
    if (question != std::string::npos) {
        request.query = request.path.substr(question + 1);
        request.path = request.path.substr(0, question);
    }

    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        request.headers.push_back({trim(line.substr(0, colon)), trim(line.substr(colon + 1))});
    }

    std::string cookieHeader = httpHeaderValue(request.headers, "cookie");
    parseCookies(request, cookieHeader);

    std::string lengthText = httpHeaderValue(request.headers, "content-length");
    std::size_t contentLength = 0;
    try { contentLength = std::stoul(lengthText); } catch (...) { contentLength = 0; }
    std::size_t already = rest.size();
    std::string body = rest;
    if (already < contentLength) {
        body += readExact(connection, contentLength - already);
    }
    request.body = body;

    std::string contentType = httpHeaderValue(request.headers, "content-type");
    std::string lowerType = lowercase(contentType);
    if (lowerType.find("application/x-www-form-urlencoded") != std::string::npos) {
        parseUrlEncoded(request, body);
    } else if (lowerType.find("multipart/form-data") != std::string::npos) {
        parseMultipart(request, body, contentType);
    }
    return true;
}

void httpWriteResponse(HttpConnection& connection, const HttpResponseData& response) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << " " << statusText(response.status) << "\r\n";

    for (const auto& header : response.headers) {
        out << header.name << ": " << header.value << "\r\n";
    }
    for (const auto& cookie : response.cookies) {
        out << "Set-Cookie: " << cookie << "\r\n";
    }

    std::string body = response.body;
    bool hasContentType = !httpHeaderValue(response.headers, "content-type").empty();
    if (!response.sendFilePath.empty()) {
        std::ifstream file(response.sendFilePath, std::ios::binary);
        if (file) {
            std::ostringstream buffer;
            buffer << file.rdbuf();
            body = buffer.str();
            if (!hasContentType) out << "Content-Type: application/octet-stream\r\n";
        } else {
            out << "HTTP/1.1 404 Not Found\r\n";
        }
    }
    if (!hasContentType && body.find('<') == 0) out << "Content-Type: text/html; charset=utf-8\r\n";
    else if (!hasContentType) out << "Content-Type: text/plain; charset=utf-8\r\n";

    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << body;

    std::string data = out.str();
    connWrite(connection, data.data(), data.size());
}

void httpServe(HttpServerState& state) {
    SSL_CTX* sslContext = nullptr;
    if (state.secure) {
        sslContext = SSL_CTX_new(TLS_server_method());
        if (!sslContext) return;
        SSL_CTX_set_min_proto_version(sslContext, TLS1_2_VERSION);
        if (SSL_CTX_use_certificate_file(sslContext, state.certFile.c_str(), SSL_FILETYPE_PEM) <= 0 ||
            SSL_CTX_use_PrivateKey_file(sslContext, state.keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
            SSL_CTX_free(sslContext);
            return;
        }
    }

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        if (sslContext) SSL_CTX_free(sslContext);
        return;
    }
    int yes = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(state.port));

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(serverFd, 128) < 0) {
        close(serverFd);
        if (sslContext) SSL_CTX_free(sslContext);
        return;
    }

    state.listenFd = serverFd;
    state.running = true;

    while (state.running) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);
        int clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
        if (clientFd < 0) {
            if (!state.running) break;
            continue;
        }

        HttpConnection connection;
        connection.fd = clientFd;
        if (sslContext) {
            connection.ssl = SSL_new(sslContext);
            if (!connection.ssl || SSL_set_fd(connection.ssl, clientFd) <= 0 ||
                SSL_accept(connection.ssl) <= 0) {
                if (connection.ssl) SSL_free(connection.ssl);
                close(clientFd);
                continue;
            }
        }

        if (state.maxConnections > 0 &&
            state.activeConnections.load() >= state.maxConnections) {
            HttpResponseData busy;
            busy.status = 503;
            busy.body = "Service Unavailable";
            httpWriteResponse(connection, busy);
            if (connection.ssl) SSL_free(connection.ssl);
            shutdown(clientFd, SHUT_WR);
            close(clientFd);
            continue;
        }
        state.activeConnections.fetch_add(1);

        HttpRequestData request;
        request.remoteAddress = inet_ntoa(clientAddress.sin_addr);
        if (!httpParseRequest(connection, request)) {
            state.activeConnections.fetch_sub(1);
            if (connection.ssl) SSL_free(connection.ssl);
            close(clientFd);
            continue;
        }

        std::thread worker([&state, connection, request]() mutable {
            HttpResponseData response;
            auto owner = state.owner.lock();
            if (owner) {
                state.mutex.lock();
                const HttpRoute* matched = nullptr;
                for (const auto& route : state.routes) {
                    if ((route.method == request.method || route.method == "ALL") &&
                        route.pattern == request.path) {
                        matched = &route;
                        break;
                    }
                }
                std::shared_ptr<FunctionValue> handler;
                if (matched) handler = matched->handler;
                state.mutex.unlock();

                if (matched && g_handlerHook && handler) {
                    g_handlerHook(*owner, *handler, request, response);
                } else if (matched) {
                    response.status = 500;
                    response.body = "Handler unavailable";
                } else {
                    response.status = 404;
                    response.body = "Not Found";
                }
            } else {
                response.status = 500;
                response.body = "Interpreter unavailable";
            }
            httpWriteResponse(connection, response);
            state.servedRequests.fetch_add(1);
            state.activeConnections.fetch_sub(1);
            if (connection.ssl) {
                SSL_shutdown(connection.ssl);
                SSL_free(connection.ssl);
            }
            shutdown(connection.fd, SHUT_WR);
            close(connection.fd);
        });
        worker.detach();
    }

    close(serverFd);
    state.listenFd = -1;
    if (sslContext) SSL_CTX_free(sslContext);
}

#include "mysql.h"

extern "C" {
struct MYSQL;
struct MYSQL_RES;

MYSQL* mysql_init(MYSQL* mysql);
void mysql_close(MYSQL* sock);
MYSQL* mysql_real_connect(MYSQL* mysql, const char* host, const char* user,
                          const char* passwd, const char* db, unsigned int port,
                          const char* unix_socket, unsigned long client_flag);
int mysql_query(MYSQL* mysql, const char* q);
MYSQL_RES* mysql_store_result(MYSQL* mysql);
unsigned int mysql_num_fields(MYSQL_RES* res);
unsigned long long mysql_num_rows(MYSQL_RES* res);
char** mysql_fetch_row(MYSQL_RES* res);
MysqlFieldView* mysql_fetch_fields(MYSQL_RES* res);
MysqlFieldView* mysql_fetch_field(MYSQL_RES* res);
unsigned long* mysql_fetch_lengths(MYSQL_RES* res);
void mysql_free_result(MYSQL_RES* res);
const char* mysql_error(MYSQL* mysql);
unsigned int mysql_errno(MYSQL* mysql);
unsigned long long mysql_insert_id(MYSQL* mysql);
unsigned long long mysql_affected_rows(MYSQL* mysql);
int mysql_set_character_set(MYSQL* mysql, const char* csname);
unsigned long mysql_real_escape_string(MYSQL* mysql, char* to, const char* from, unsigned long length);
int mysql_ping(MYSQL* mysql);
}

namespace {

void setError(MysqlConnectionData& connection) {
    connection.lastError = mysql_error(connection.handle)
        ? mysql_error(connection.handle) : "unknown error";
}

}

bool mysqlCheckOwner(MysqlConnectionData& connection) {
    if (!connection.handle) return false;
    if (connection.ownerThread == std::this_thread::get_id()) return true;
    connection.lastError = "MySQL connection belongs to another thread";
    return false;
}

std::shared_ptr<MysqlConnectionData> mysqlConnect(const std::string& host, const std::string& user,
                                                  const std::string& password, const std::string& database,
                                                  int port) {
    auto connection = std::make_shared<MysqlConnectionData>();
    connection->handle = mysql_init(nullptr);
    if (!connection->handle) {
        connection->lastError = "mysql_init failed";
        return connection;
    }
    const char* hostPtr = host.empty() ? nullptr : host.c_str();
    const char* userPtr = user.empty() ? nullptr : user.c_str();
    const char* passwordPtr = password.empty() ? nullptr : password.c_str();
    const char* databasePtr = database.empty() ? nullptr : database.c_str();

    if (!mysql_real_connect(connection->handle, hostPtr, userPtr, passwordPtr,
                            databasePtr, static_cast<unsigned int>(port), nullptr, 0)) {
        setError(*connection);
        mysql_close(connection->handle);
        connection->handle = nullptr;
        return connection;
    }
    mysql_set_character_set(connection->handle, "utf8mb4");
    connection->ownerThread = std::this_thread::get_id();
    return connection;
}

void mysqlClose(MysqlConnectionData& connection) {
    if (connection.handle) {
        mysql_close(connection.handle);
        connection.handle = nullptr;
    }
}

bool mysqlExecute(MysqlConnectionData& connection, const std::string& query, MysqlResultData& result) {
    if (!mysqlCheckOwner(connection)) {
        connection.lastError = "not connected";
        return false;
    }
    if (mysql_query(connection.handle, query.c_str()) != 0) {
        setError(connection);
        return false;
    }

    result.affectedRows = mysql_affected_rows(connection.handle);
    result.insertId = mysql_insert_id(connection.handle);

    MYSQL_RES* rawResult = mysql_store_result(connection.handle);
    if (!rawResult) {
        if (mysql_errno(connection.handle) != 0) {
            setError(connection);
            return false;
        }
        return true;
    }

    unsigned int columnCount = mysql_num_fields(rawResult);
    for (unsigned int i = 0; i < columnCount; ++i) {
        MysqlFieldView* field = mysql_fetch_field(rawResult);
        result.columns.push_back(field && field->name ? field->name : "");
    }

    while (true) {
        char** row = mysql_fetch_row(rawResult);
        if (!row) break;
        unsigned long* lengths = mysql_fetch_lengths(rawResult);
        MysqlResultRow parsed;
        parsed.columns = result.columns;
        for (unsigned int i = 0; i < columnCount; ++i) {
            std::size_t length = lengths ? lengths[i] : std::string(row[i] ? row[i] : "").size();
            parsed.values.push_back(row[i] ? std::string(row[i], length) : std::string());
        }
        result.rows.push_back(std::move(parsed));
    }

    mysql_free_result(rawResult);
    return true;
}

std::string mysqlEscape(MysqlConnectionData& connection, const std::string& value) {
    if (!mysqlCheckOwner(connection)) return value;
    std::vector<char> buffer(value.size() * 2 + 1);
    unsigned long length = mysql_real_escape_string(connection.handle, buffer.data(),
                                                    value.c_str(), static_cast<unsigned long>(value.size()));
    return std::string(buffer.data(), length);
}

std::string mysqlLastError(MysqlConnectionData& connection) {
    return connection.lastError;
}

unsigned long long mysqlInsertId(MysqlConnectionData& connection) {
    return mysqlCheckOwner(connection) ? mysql_insert_id(connection.handle) : 0;
}

unsigned long long mysqlAffectedRows(MysqlConnectionData& connection) {
    return mysqlCheckOwner(connection) ? mysql_affected_rows(connection.handle) : 0;
}

bool mysqlPing(MysqlConnectionData& connection) {
    return mysqlCheckOwner(connection) && mysql_ping(connection.handle) == 0;
}

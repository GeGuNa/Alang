#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

struct MYSQL;
struct MYSQL_RES;

struct MysqlFieldView {
    const char* name;
};

struct MysqlResultRow {
    std::vector<std::string> columns;
    std::vector<std::string> values;
};

struct MysqlResultData {
    std::vector<std::string> columns;
    std::vector<MysqlResultRow> rows;
    unsigned long long affectedRows = 0;
    unsigned long long insertId = 0;
};

struct MysqlConnectionData {
    MYSQL* handle = nullptr;
    std::thread::id ownerThread;
    std::string lastError;
};

bool mysqlCheckOwner(MysqlConnectionData& connection);

std::shared_ptr<MysqlConnectionData> mysqlConnect(const std::string& host, const std::string& user,
                                                  const std::string& password, const std::string& database,
                                                  int port);
void mysqlClose(MysqlConnectionData& connection);
bool mysqlExecute(MysqlConnectionData& connection, const std::string& query, MysqlResultData& result);
std::string mysqlEscape(MysqlConnectionData& connection, const std::string& value);
std::string mysqlLastError(MysqlConnectionData& connection);
unsigned long long mysqlInsertId(MysqlConnectionData& connection);
unsigned long long mysqlAffectedRows(MysqlConnectionData& connection);
bool mysqlPing(MysqlConnectionData& connection);

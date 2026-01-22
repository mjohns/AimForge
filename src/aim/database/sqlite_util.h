#pragma once

#include <string>

#include "aim/common/log.h"
#include "sqlite3.h"

namespace aim {

static bool ExecuteSqliteQuery(sqlite3* db,
                               const char* sql,
                               std::string* error_message_out = nullptr) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("SQL error: {}", err_msg);
    if (error_message_out != nullptr) {
      *error_message_out = std::format("SQL error: {}", err_msg);
    }
    sqlite3_free(err_msg);
    return false;
  }
  return true;
}

static void BindString(sqlite3_stmt* stmt, int index, const std::string& value) {
  sqlite3_bind_text(stmt, index, value.c_str(), value.size(), SQLITE_TRANSIENT);
}

static bool IsColumnNull(sqlite3_stmt* stmt, int index) {
  return sqlite3_column_type(stmt, index) == SQLITE_NULL;
}

}  // namespace aim

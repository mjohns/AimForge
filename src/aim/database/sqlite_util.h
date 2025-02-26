#pragma once

#include <sqlite3.h>

#include <string>

namespace aim {

static bool ExecuteSqliteQuery(sqlite3* db, const char* sql) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return false;
  }
  return true;
}

static void BindString(sqlite3_stmt* stmt, int index, const std::string& value) {
  sqlite3_bind_text(stmt, index, value.c_str(), value.size(), SQLITE_TRANSIENT);
}

}  // namespace aim

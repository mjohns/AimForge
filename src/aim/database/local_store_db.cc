#include "local_store_db.h"

#include <format>
#include <string>

#include "aim/common/log.h"
#include "aim/common/times.h"
#include "aim/database/sqlite_util.h"
#include "google/protobuf/json/json.h"
#include "google/protobuf/util/json_util.h"
#include "sqlite3.h"

namespace aim {
namespace {

const char* kCreateKeyValuesTable = R"AIMS(
CREATE TABLE IF NOT EXISTS KeyValues (
    Key TEXT,
    Value TEXT,
    PRIMARY KEY (Key)
);
)AIMS";

const char* kInsertValueSql = R"AIMS(
INSERT OR REPLACE INTO KeyValues (Key, Value) VALUES ( ?, ?)
ON CONFLICT (Key) DO UPDATE SET Value = ?;
)AIMS";

const char* kDeleteValueSql = R"AIMS(
DELETE FROM KeyValues WHERE Key = ?;
)AIMS";

const char* kGetValueSql = R"AIMS(
SELECT Value FROM KeyValues WHERE Key = ?;
)AIMS";

}  // namespace

LocalStoreDb::LocalStoreDb(const std::filesystem::path& db_path) {
  char* err_msg = 0;
  std::string db_path_str = db_path.string();
  int rc = sqlite3_open(db_path_str.c_str(), &db_);

  if (rc != SQLITE_OK) {
    Logger::get()->warn("Cannot open local store db: {}", sqlite3_errmsg(db_));
    sqlite3_close(db_);
    db_ = nullptr;
  }

  ExecuteSqliteQuery(db_, kCreateKeyValuesTable);
}

LocalStoreDb::~LocalStoreDb() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

void LocalStoreDb::PutValue(const std::string& key, const std::string& value) {
  if (value.size() == 0) {
    RemoveKey(key);
    return;
  }
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kInsertValueSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to prepare statement: {}", sqlite3_errmsg(db_));
    return;
  }

  BindString(stmt, 1, key);
  BindString(stmt, 2, value);
  BindString(stmt, 3, value);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    Logger::get()->warn("Failed to put key {}: {}", key, sqlite3_errmsg(db_));
  }
  sqlite3_finalize(stmt);
}

void LocalStoreDb::RemoveKey(const std::string& key) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kDeleteValueSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return;
  }
  BindString(stmt, 1, key);
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    Logger::get()->warn("Failed to delete key {}: {}", key, sqlite3_errmsg(db_));
  }
  sqlite3_finalize(stmt);
}

std::string LocalStoreDb::GetValue(const std::string& key) {
  sqlite3_stmt* stmt;

  int rc = sqlite3_prepare_v2(db_, kGetValueSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return {};
  }
  BindString(stmt, 1, key);

  std::string value;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
  }

  sqlite3_finalize(stmt);
  return value;
}

}  // namespace aim

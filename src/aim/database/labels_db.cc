#include "labels_db.h"

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

const char* kCreateLabeledItemsTable = R"AIMS(
CREATE TABLE IF NOT EXISTS LabeledItems (
    Label TEXT,
    Type TEXT,
    Id TEXT,
    PRIMARY KEY (Label, Type, Id)
);
)AIMS";

const char* kInsertLabeledItemSql = R"AIMS(
INSERT OR REPLACE INTO LabeledItems (Label, Type, Id) VALUES ( ?, ?, ?)
ON CONFLICT DO NOTHING;
)AIMS";

const char* kDeleteLabeledItemSql = R"AIMS(
DELETE FROM LabeledItems WHERE Label = ? AND Type = ? AND Id = ?;
)AIMS";

const char* kListLabeledItemsSql = R"AIMS(
SELECT Id FROM LabeledItems WHERE Label = ? AND Type = ?;
)AIMS";

}  // namespace

LabelsDb::LabelsDb(const std::filesystem::path& db_path) {
  char* err_msg = 0;
  std::string db_path_str = db_path.string();
  int rc = sqlite3_open(db_path_str.c_str(), &db_);

  if (rc != SQLITE_OK) {
    Logger::get()->warn("Cannot open labels db: {}", sqlite3_errmsg(db_));
    sqlite3_close(db_);
    db_ = nullptr;
  }

  ExecuteSqliteQuery(db_, kCreateLabeledItemsTable);
}

LabelsDb::~LabelsDb() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

void LabelsDb::AddLabeledItem(const std::string& label,
                              ObjectType type,
                              const std::string& object_id) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kInsertLabeledItemSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to prepare statement: {}", sqlite3_errmsg(db_));
    return;
  }

  std::string type_string = ObjectTypeToString(type);

  BindString(stmt, 1, label);
  BindString(stmt, 2, type_string);
  BindString(stmt, 3, object_id);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void LabelsDb::RemoveLabeledItem(const std::string& label,
                                 ObjectType type,
                                 const std::string& object_id) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kDeleteLabeledItemSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return;
  }
  BindString(stmt, 1, label);
  std::string type_string = ObjectTypeToString(type);
  BindString(stmt, 2, type_string);
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    Logger::get()->warn("Failed to delete labeled item for {}: {}", label, sqlite3_errmsg(db_));
  }
  sqlite3_finalize(stmt);
}

std::vector<std::string> LabelsDb::ListLabeledItems(const std::string& label, ObjectType type) {
  sqlite3_stmt* stmt;

  int rc = sqlite3_prepare_v2(db_, kListLabeledItemsSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return {};
  }
  BindString(stmt, 1, label);
  std::string type_string = ObjectTypeToString(type);
  BindString(stmt, 2, type_string);

  std::vector<std::string> ids;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    ids.push_back(id);
  }

  sqlite3_finalize(stmt);
  return ids;
}

}  // namespace aim

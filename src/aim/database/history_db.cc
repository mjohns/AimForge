#include "history_db.h"

#include <google/protobuf/json/json.h>
#include <google/protobuf/util/json_util.h>
#include <sqlite3.h>

#include <format>
#include <string>

#include "aim/common/log.h"
#include "aim/common/times.h"
#include "aim/database/sqlite_util.h"

namespace aim {
namespace {

const char* kCreateRecentViewsTable = R"AIMS(
CREATE TABLE IF NOT EXISTS RecentViews (
    Type TEXT,
    Id TEXT,
    Timestamp TEXT,
    ExtraInfo TEXT,
    PRIMARY KEY (Type, Id)
);
)AIMS";

const char* kInsertRecentViewsSql = R"AIMS(
INSERT INTO RecentViews (Type, Id, Timestamp) VALUES (?, ?, ?)
ON CONFLICT (Type, Id) DO UPDATE SET Timestamp = ?;
)AIMS";

const char* kGetRecentViewsForTypeSql = R"AIMS(
SELECT Id, Timestamp
FROM RecentViews
WHERE Type = ?
ORDER BY Timestamp DESC
LIMIT ?;
)AIMS";

std::string RecentViewTypeToString(RecentViewType t) {
  switch (t) {
    case RecentViewType::PLAYLIST:
      return "Playlist";
    case RecentViewType::SCENARIO:
      return "Scenario";
    case RecentViewType::THEME:
      return "Theme";
    case RecentViewType::CROSSHAIR:
      return "Crosshair";
  }
  assert(false);
}

}  // namespace

HistoryDb::HistoryDb(const std::filesystem::path& db_path) {
  char* err_msg = 0;
  std::string db_path_str = db_path.string();
  int rc = sqlite3_open(db_path_str.c_str(), &db_);

  if (rc != SQLITE_OK) {
    Logger::get()->warn("Cannot open history db: {}", sqlite3_errmsg(db_));
    sqlite3_close(db_);
    db_ = nullptr;
  }

  ExecuteSqliteQuery(db_, kCreateRecentViewsTable);
}

HistoryDb::~HistoryDb() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

void HistoryDb::UpdateRecentView(RecentViewType t, const std::string& id) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kInsertRecentViewsSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to prepare statement: {}", sqlite3_errmsg(db_));
    return;
  }

  std::string type_string = RecentViewTypeToString(t);
  std::string timestamp = GetNowString();

  BindString(stmt, 1, type_string);
  BindString(stmt, 2, id);
  BindString(stmt, 3, timestamp);
  BindString(stmt, 4, timestamp);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

std::vector<RecentView> HistoryDb::GetRecentViews(RecentViewType t, int limit) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kGetRecentViewsForTypeSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return {};
  }

  std::string type_string = RecentViewTypeToString(t);
  BindString(stmt, 1, type_string);
  sqlite3_bind_int(stmt, 2, limit);

  std::vector<RecentView> views;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    views.push_back({});
    RecentView& view = views.back();
    view.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    view.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  }

  sqlite3_finalize(stmt);
  return views;
}

}  // namespace aim

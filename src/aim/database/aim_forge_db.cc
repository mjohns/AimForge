#include "aim_forge_db.h"

#include <format>
#include <string>

#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/times.h"
#include "aim/database/sqlite_util.h"
#include "sqlite3.h"

namespace aim {
namespace {

const char* kCreateScenariosTable = R"AIMS(
CREATE TABLE IF NOT EXISTS Scenarios (
    ScenarioId INTEGER PRIMARY KEY AUTOINCREMENT,
    ScenarioName TEXT NOT NULL,
    Settings BLOB
);
)AIMS";

const char* kCreateScenarioSql = R"AIMS(
INSERT INTO Scenarios (ScenarioId, ScenarioName) VALUES (NULL, ?);
)AIMS";

const char* kGetAllScenarioIdsSql = R"AIMS(
SELECT ScenarioId, ScenarioName FROM Scenarios;
)AIMS";

const char* kCreateScenarioNameIndex = R"AIMS(
CREATE UNIQUE INDEX IF NOT EXISTS ScenariosByName ON Scenarios(ScenarioName);
)AIMS";

// Stores the label (starred) for playlists/scenarios with the specified integer id of the item.
const char* kLabeledItemsTable = R"AIMS(
CREATE TABLE IF NOT EXISTS LabeledItems (
    Label INTEGER NOT NULL,
    Type INTEGER NOT NULL,
    Id INTEGER NOT NULL,
    PRIMARY KEY (Label, Type, Id)
);
)AIMS";

const char* kCreateStatsTable = R"AIMS(
CREATE TABLE IF NOT EXISTS Stats (
    ScenarioId INTEGER PRIMARY_KEY,
    StatsId INTEGER PRIMARY KEY AUTOINCREMENT,
    TimestampSeconds INTEGER NOT NULL,
    Score REAL,
    MmPer360 INTEGER,
    Info BLOB,
);
)AIMS";

class AimForgeDbImpl : public AimForgeDb {
 public:
  explicit AimForgeDbImpl(const std::filesystem::path& db_path) {
    char* err_msg = 0;
    std::string db_path_str = db_path.string();
    int rc = sqlite3_open(db_path_str.c_str(), &db_);

    if (rc != SQLITE_OK) {
      Logger::get()->warn("Cannot open AimForge db: {}", sqlite3_errmsg(db_));
      sqlite3_close(db_);
      db_ = nullptr;
      return;
    }

    ExecuteSqliteQuery(db_, kCreateScenariosTable);
  }

  i64 CreateScenarioEntry(const std::string& name) override {
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, kCreateScenarioSql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
      Logger::get()->warn("Failed to prepare statement: {}", sqlite3_errmsg(db_));
      return -1;
    }
    BindString(stmt, 1, name);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return sqlite3_last_insert_rowid(db_);
  }

  std::unordered_map<std::string, i64> GetScenarioIdMap() override {
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, kGetAllScenarioIdsSql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
      Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
      return {};
    }

    std::unordered_map<std::string, i64> name_to_id_map;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      i64 id = sqlite3_column_int64(stmt, 0);
      name_to_id_map[reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))] = id;
    }

    sqlite3_finalize(stmt);
    return name_to_id_map;
  }

  ~AimForgeDbImpl() override {
    if (db_ != nullptr) {
      sqlite3_close(db_);
    }
  }

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace

std::unique_ptr<AimForgeDb> CreateAimForgeDb(const std::filesystem::path& db_path) {
  return std::make_unique<AimForgeDbImpl>(db_path);
}

}  // namespace aim

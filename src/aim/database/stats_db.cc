#include "stats_db.h"

#include <sqlite3.h>

#include <format>
#include <string>

#include "aim/common/log.h"
#include "aim/common/times.h"
#include "aim/database/sqlite_util.h"

namespace aim {
namespace {

const char* kCreateStatsTable = R"AIMS(
CREATE TABLE IF NOT EXISTS Stats (
    ScenarioId TEXT PRIMARY_KEY,
    StatsId INTEGER PRIMARY KEY AUTOINCREMENT,
    Timestamp TEXT,
    Score REAL,
    CmPer360 REAL,
    NumHits REAL,
    NumShots REAL,
    ExtraInfo TEXT
);
)AIMS";

const char* kInsertSql = R"AIMS(
INSERT INTO Stats (
	StatsId,
    ScenarioId,
    Timestamp,
    NumHits,
    NumShots,
    CmPer360,
    Score)
  VALUES (NULL, ?, ?, ?, ?, ?, ?);
)AIMS";

const char* kGetRecentStatsSql = R"AIMS(
SELECT 
  StatsId,
  Timestamp,
  NumHits,
  NumShots,
  CmPer360,
  Score
FROM Stats
WHERE ScenarioId = ?
ORDER BY StatsId ASC LIMIT 5000;
)AIMS";

const char* kGetMostRecentRunIdSql = R"AIMS(
SELECT StatsId
FROM Stats
WHERE ScenarioId = ?
ORDER BY StatsId DESC LIMIT 1;
)AIMS";

const char* kDeleteAllStatsForScenarioSql = R"AIMS(
DELETE FROM Stats WHERE ScenarioId = ?;
)AIMS";

const char* kRenameScenarioSql = R"AIMS(
UPDATE Stats SET ScenarioId = ? WHERE ScenarioId = ?;
)AIMS";

}  // namespace

StatsDb::StatsDb(const std::filesystem::path& db_path) {
  char* err_msg = 0;
  std::string db_path_str = db_path.string();
  int rc = sqlite3_open(db_path_str.c_str(), &db_);

  if (rc != SQLITE_OK) {
    Logger::get()->warn("Cannot open stats db: {}", sqlite3_errmsg(db_));
    sqlite3_close(db_);
    db_ = nullptr;
  }

  ExecuteSqliteQuery(db_, kCreateStatsTable);
}

StatsDb::~StatsDb() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

std::vector<StatsRow> StatsDb::GetStats(const std::string& scenario_id) {
  sqlite3_stmt* stmt;

  int rc = sqlite3_prepare_v2(db_, kGetRecentStatsSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return {};
  }
  BindString(stmt, 1, scenario_id);

  std::vector<StatsRow> all_stats;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    StatsRow stats;
    stats.stats_id = sqlite3_column_int64(stmt, 0);
    stats.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    stats.num_hits = sqlite3_column_double(stmt, 2);
    stats.num_shots = sqlite3_column_double(stmt, 3);
    stats.cm_per_360 = sqlite3_column_double(stmt, 4);
    stats.score = sqlite3_column_double(stmt, 5);

    all_stats.push_back(stats);
  }

  sqlite3_finalize(stmt);
  return all_stats;
}

i64 StatsDb::GetLatestRunId(const std::string& scenario_id) {
  sqlite3_stmt* stmt;

  int rc = sqlite3_prepare_v2(db_, kGetMostRecentRunIdSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return {};
  }
  BindString(stmt, 1, scenario_id);

  i64 run_id = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    run_id = sqlite3_column_int64(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return run_id;
}

void StatsDb::AddStats(const std::string& scenario_id, StatsRow* row) {
  if (row->timestamp.size() == 0) {
    row->timestamp = GetNowString();
  }

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to prepare statement: {}", sqlite3_errmsg(db_));
    return;
  }

  BindString(stmt, 1, scenario_id);
  BindString(stmt, 2, row->timestamp);
  sqlite3_bind_double(stmt, 3, row->num_hits);
  sqlite3_bind_double(stmt, 4, row->num_shots);
  sqlite3_bind_double(stmt, 5, row->cm_per_360);
  sqlite3_bind_double(stmt, 6, row->score);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  // return rc == SQLITE_DONE;

  row->stats_id = sqlite3_last_insert_rowid(db_);
  return;
}

void StatsDb::DeleteAllStats(const std::string& scenario_id) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kDeleteAllStatsForScenarioSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return;
  }
  BindString(stmt, 1, scenario_id);
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    Logger::get()->warn("Failed to delete stats for {}: {}", scenario_id, sqlite3_errmsg(db_));
  }
  sqlite3_finalize(stmt);
}

void StatsDb::RenameScenario(const std::string& old_scenario_id,
                             const std::string& new_scenario_id) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kRenameScenarioSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return;
  }
  BindString(stmt, 1, new_scenario_id);
  BindString(stmt, 2, old_scenario_id);
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    Logger::get()->warn(
        "Failed to rename scenario id {}: {}", old_scenario_id, sqlite3_errmsg(db_));
  }
  sqlite3_finalize(stmt);
}

void StatsDb::CopyAllStats(const std::string& from_scenario_id, const std::string& to_scenario_id) {
}

void StatsDb::DeleteStats(const std::string& scenario_id, i64 run_id) {}

}  // namespace aim

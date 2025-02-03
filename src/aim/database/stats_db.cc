#include "stats_db.h"

#include <sqlite3.h>

#include <format>
#include <string>

#include "aim/common/time_util.h"

namespace aim {
namespace {

const char* kCreateStatsTable = R"AIMS(
CREATE TABLE IF NOT EXISTS Stats (
    ScenarioId TEXT PRIMARY_KEY,
    StatsId INTEGER PRIMARY KEY AUTOINCREMENT,
    Timestamp TEXT,
    Score REAL,
    NumHits INTEGER,
    NumKills INTEGER,
    NumShots INTEGER
);
)AIMS";

const char* kInsertSql = R"AIMS(
INSERT INTO Stats (
	StatsId,
    ScenarioId,
    Timestamp,
    NumHits,
    NumShots,
    NumKills,
    Score)
  VALUES (NULL, ?, ?, ?, ?, ?, ?);
)AIMS";

const char* kGetRecentStatsSql = R"AIMS(
SELECT 
  StatsId,
  Timestamp,
  NumHits,
  NumShots,
  NumKills,
  Score
FROM Stats ORDER BY ScenarioId DESC LIMIT 2000;
)AIMS";

bool ExecuteQuery(sqlite3* db, const char* sql) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    return false;
  }
  return true;
}

void BindString(sqlite3_stmt* stmt, int index, const std::string& value) {
  sqlite3_bind_text(stmt, index, value.c_str(), value.size(), SQLITE_TRANSIENT);
}

}  // namespace

StatsDb::StatsDb() {
  char* err_msg = 0;
  int rc = sqlite3_open("C:/Users/micha/AimTrainer/stats.db", &db_);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_));
    sqlite3_close(db_);
    db_ = nullptr;
  }

  ExecuteQuery(db_, kCreateStatsTable);
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
    fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db_));
    return {};
  }

  std::vector<StatsRow> all_stats;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    StatsRow stats;
    stats.stats_id = sqlite3_column_int64(stmt, 0);
    stats.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    stats.num_hits = sqlite3_column_int64(stmt, 2);
    stats.num_shots = sqlite3_column_int64(stmt, 3);
    stats.num_kills = sqlite3_column_int64(stmt, 4);
    stats.score = sqlite3_column_double(stmt, 5);

    all_stats.push_back(stats);
  }

  sqlite3_finalize(stmt);
  return all_stats;
}

void StatsDb::AddStats(const std::string& scenario_id, StatsRow* row) {
  if (row->timestamp.size() == 0) {
    row->timestamp = GetNowString();
  }

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db_));
    return;
  }

  BindString(stmt, 1, scenario_id);
  BindString(stmt, 2, row->timestamp);
  sqlite3_bind_int(stmt, 3, row->num_hits);
  sqlite3_bind_int(stmt, 4, row->num_shots);
  sqlite3_bind_int(stmt, 5, row->num_kills);
  sqlite3_bind_double(stmt, 6, row->score);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  // return rc == SQLITE_DONE;

  row->stats_id = sqlite3_last_insert_rowid(db_);
  return;
}

}  // namespace aim

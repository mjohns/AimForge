#include "play_time_db.h"

#include <sqlite3.h>

#include <format>
#include <string>

#include "aim/common/log.h"
#include "aim/common/times.h"
#include "aim/database/sqlite_util.h"

namespace aim {
namespace {

const char* kCreatePlayTimeTable = R"AIMS(
CREATE TABLE IF NOT EXISTS PlayTime (
    PlayTimeId INTEGER PRIMARY KEY AUTOINCREMENT,
    Timestamp TEXT,
    DurationSeconds REAL,
    ScenarioId TEXT,
    CmPer360 REAL,
    IsCompleteRun INTEGER,
    ShotType TEXT
);
)AIMS";

const char* kInsertSql = R"AIMS(
INSERT INTO PlayTime (
	  PlayTimeId,
    Timestamp,
    DurationSeconds,
    ScenarioId,
    CmPer360,
    IsCompleteRun,
    ShotType)
  VALUES (NULL, ?, ?, ?, ?, ?, ?);
)AIMS";

std::string ShotTypeToString(ShotType::TypeCase type) {
  switch (type) {
    case ShotType::kClickSingle:
      return "Click";
    case ShotType::kPoke:
      return "Poke";
    case ShotType::kTrackingInvincible:
      return "Tracking";
    case ShotType::kTrackingKill:
      return "TrackingKill";
  }
  return "";
}

}  // namespace

PlayTimeDb::PlayTimeDb(const std::filesystem::path& db_path) {
  char* err_msg = 0;
  std::string db_path_str = db_path.string();
  int rc = sqlite3_open(db_path_str.c_str(), &db_);

  if (rc != SQLITE_OK) {
    Logger::get()->warn("Cannot open play time db: {}", sqlite3_errmsg(db_));
    sqlite3_close(db_);
    db_ = nullptr;
  }

  ExecuteSqliteQuery(db_, kCreatePlayTimeTable);
}

PlayTimeDb::~PlayTimeDb() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

void PlayTimeDb::AddPlayTime(const PlayTime& play_time) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to prepare statement: {}", sqlite3_errmsg(db_));
    return;
  }

  BindString(stmt, 1, GetNowString());
  sqlite3_bind_double(stmt, 2, play_time.duration_seconds);
  BindString(stmt, 3, play_time.scenario_id);
  sqlite3_bind_double(stmt, 4, play_time.cm_per_360);
  sqlite3_bind_int(stmt, 5, play_time.is_complete_run);
  BindString(stmt, 6, ShotTypeToString(play_time.shot_type));

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  // return rc == SQLITE_DONE;
}

float PlayTimeDb::GetTotalPlayTimeSeconds() {
  return 0;
}

}  // namespace aim

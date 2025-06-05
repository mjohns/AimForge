#include "settings_db.h"

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

const char* kCreateScenarioSettingsTable = R"AIMS(
CREATE TABLE IF NOT EXISTS ScenarioSettings (
    ScenarioId TEXT,
    Settings TEXT,
    PRIMARY KEY (ScenarioId)
);
)AIMS";

const char* kInsertSql = R"AIMS(
INSERT OR REPLACE INTO ScenarioSettings (ScenarioId, Settings) VALUES ( ?, ?);
ON CONFLICT (ScenarioId) DO UPDATE SET Settings = ?;
)AIMS";

const char* kGetScenarioSettingsSql = R"AIMS(
SELECT Settings
FROM ScenarioSettings
WHERE ScenarioId = ?;
)AIMS";

}  // namespace

SettingsDb::SettingsDb(const std::filesystem::path& db_path) {
  char* err_msg = 0;
  std::string db_path_str = db_path.string();
  int rc = sqlite3_open(db_path_str.c_str(), &db_);

  if (rc != SQLITE_OK) {
    Logger::get()->warn("Cannot open settings db: {}", sqlite3_errmsg(db_));
    sqlite3_close(db_);
    db_ = nullptr;
  }

  ExecuteSqliteQuery(db_, kCreateScenarioSettingsTable);
}

SettingsDb::~SettingsDb() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

void SettingsDb::UpdateScenarioSettings(const std::string& scenario_id,
                                        const ScenarioSettings& settings) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to prepare statement: {}", sqlite3_errmsg(db_));
    return;
  }

  std::string json_string;
  google::protobuf::json::PrintOptions opts;
  opts.add_whitespace = true;
  opts.unquote_int64_if_possible = true;
  auto status = google::protobuf::util::MessageToJsonString(settings, &json_string, opts);
  if (!status.ok()) {
    Logger::get()->error("Unable to serialize settings message to json: {}", status.message());
    return;
  }

  BindString(stmt, 1, scenario_id);
  BindString(stmt, 2, json_string);
  BindString(stmt, 3, json_string);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

std::optional<ScenarioSettings> SettingsDb::GetScenarioSettings(const std::string& scenario_id) {
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, kGetScenarioSettingsSql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    Logger::get()->warn("Failed to fetch data: {}", sqlite3_errmsg(db_));
    return {};
  }
  BindString(stmt, 1, scenario_id);

  std::optional<ScenarioSettings> maybe_settings;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string json_text(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));

    google::protobuf::json::ParseOptions opts;
    opts.ignore_unknown_fields = true;
    opts.case_insensitive_enum_parsing = true;
    ScenarioSettings settings;
    auto status = google::protobuf::util::JsonStringToMessage(json_text, &settings, opts);
    if (status.ok()) {
      maybe_settings = settings;
    } else {
      Logger::get()->warn("Unable to parse settings json ({}): {}", status.message(), json_text);
    }
  }

  sqlite3_finalize(stmt);
  return maybe_settings;
}

}  // namespace aim

#pragma once

#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <vector>

#include "aim/proto/settings.pb.h"

namespace aim {

class SettingsDb {
 public:
  explicit SettingsDb(const std::filesystem::path& db_path);
  ~SettingsDb();

  void UpdateScenarioSettings(const std::string& scenario_id, const ScenarioSettings& settings);

  ScenarioSettings GetScenarioSettings(const std::string& scenario_id);

  SettingsDb(const SettingsDb&) = delete;
  SettingsDb(SettingsDb&&) = default;
  SettingsDb& operator=(SettingsDb other) = delete;
  SettingsDb& operator=(SettingsDb&& other) = delete;

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace aim

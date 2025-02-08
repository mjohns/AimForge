#pragma once

#include <filesystem>
#include "aim/fbs/settings_generated.h"

namespace aim {

class SettingsManager {
 public:
  explicit SettingsManager(const std::filesystem::path& settings_path);

  const SettingsT& GetSettings();
  ScenarioSettingsT GetScenarioSettings(const std::string& scenario_id);
  void WriteSettings(const SettingsT& settings);

  SettingsManager(const SettingsManager&) = delete;
  SettingsManager(SettingsManager&&) = default;
  SettingsManager& operator=(SettingsManager other) = delete;
  SettingsManager& operator=(SettingsManager&& other) = delete;

 private:
  std::filesystem::path settings_path_;
  SettingsT settings_;
};

}  // namespace aim

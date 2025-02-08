#pragma once

#include <filesystem>

#include "aim/fbs/settings_generated.h"

namespace aim {

class SettingsManager {
 public:
  explicit SettingsManager(const std::filesystem::path& settings_path);

  FullSettingsT GetFullSettings();
  float GetDpi();
  SettingsT GetCurrentSettings();

  void WriteSettings(const FullSettingsT& settings);

  SettingsManager(const SettingsManager&) = delete;
  SettingsManager(SettingsManager&&) = default;
  SettingsManager& operator=(SettingsManager other) = delete;
  SettingsManager& operator=(SettingsManager&& other) = delete;

 private:
  std::filesystem::path settings_path_;
  FullSettingsT full_settings_;
};

}  // namespace aim

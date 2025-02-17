#pragma once

#include <absl/status/status.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/proto/settings.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {

class SettingsManager {
 public:
  explicit SettingsManager(const std::filesystem::path& settings_path,
                           std::vector<std::filesystem::path> theme_dirs);
  ~SettingsManager();

  absl::Status Initialize();

  FullSettings GetFullSettings();
  FullSettings* GetMutableFullSettings();
  float GetDpi();
  Settings GetCurrentSettings();
  Settings* GetMutableCurrentSettings();

  Theme GetTheme(const std::string& theme_name);
  Theme GetCurrentTheme();

  void MarkDirty();
  void FlushToDisk();
  // Only flush to disk if marked dirty.
  void MaybeFlushToDisk();

  SettingsManager(const SettingsManager&) = delete;
  SettingsManager(SettingsManager&&) = default;
  SettingsManager& operator=(SettingsManager other) = delete;
  SettingsManager& operator=(SettingsManager&& other) = delete;

 private:
  std::filesystem::path settings_path_;
  FullSettings full_settings_;
  bool needs_save_ = false;
  std::vector<std::filesystem::path> theme_dirs_;
  std::unordered_map<std::string, Theme> theme_cache_;
};

}  // namespace aim

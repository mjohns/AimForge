#pragma once

#include <absl/status/status.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/times.h"
#include "aim/proto/settings.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {

struct ThemeCacheEntry {
  Theme theme;
  std::filesystem::path file_path;
  std::filesystem::file_time_type last_modified_time;
};

class SettingsManager {
 public:
  explicit SettingsManager(const std::filesystem::path& settings_path,
                           std::vector<std::filesystem::path> theme_dirs);
  ~SettingsManager();

  absl::Status Initialize();

  float GetDpi();
  Settings GetCurrentSettings();
  Settings* GetMutableCurrentSettings();

  Theme GetTheme(const std::string& theme_name);
  Theme GetCurrentTheme();

  void MarkDirty();
  void FlushToDisk();
  // Only flush to disk if marked dirty.
  void MaybeFlushToDisk();

  void MaybeInvalidateThemeCache();

  SettingsManager(const SettingsManager&) = delete;
  SettingsManager(SettingsManager&&) = default;
  SettingsManager& operator=(SettingsManager other) = delete;
  SettingsManager& operator=(SettingsManager&& other) = delete;

 private:
  Theme GetThemeNoReferenceFollow(const std::string& theme_name);

  std::filesystem::path settings_path_;
  Settings settings_;
  bool needs_save_ = false;
  std::vector<std::filesystem::path> theme_dirs_;
  std::unordered_map<std::string, ThemeCacheEntry> theme_cache_;
};

struct SettingsUpdater {
 public:
  explicit SettingsUpdater(SettingsManager* settings_manager);

  void SaveIfChangesMade();
  void SaveIfChangesMadeDebounced(float debounce_seconds);

  std::string cm_per_360;
  std::string theme_name;
  std::string metronome_bpm;
  std::string crosshair_size;

 private:
  SettingsManager* settings_manager_;
  Stopwatch last_update_timer_;
};

}  // namespace aim

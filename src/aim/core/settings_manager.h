#pragma once

#include <absl/status/status.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/times.h"
#include "aim/database/history_db.h"
#include "aim/database/settings_db.h"
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
                           std::vector<std::filesystem::path> theme_dirs,
                           SettingsDb* settings_db,
                           HistoryDb* history_db);
  ~SettingsManager();

  absl::Status Initialize();

  float GetDpi();
  Settings GetCurrentSettings();
  Settings GetCurrentSettingsForScenario(const std::string& scenario_id);
  Settings* GetMutableCurrentSettings();

  Theme GetTheme(const std::string& theme_name);
  Theme GetCurrentTheme();

  std::vector<std::string> ListThemes();

  Crosshair GetCurrentCrosshair();

  void MarkDirty();
  void FlushToDisk(const std::string& scenario_id);
  // Only flush to disk if marked dirty.
  bool MaybeFlushToDisk(const std::string& scenario_id);

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
  std::unordered_map<std::string, ScenarioSettings> scenario_settings_cache_;
  SettingsDb* settings_db_;
  HistoryDb* history_db_;
};

struct SettingsUpdater {
 public:
  explicit SettingsUpdater(SettingsManager* settings_manager, HistoryDb* history_db);

  void SaveIfChangesMade(const std::string& scenario_id);
  void SaveIfChangesMadeDebounced(const std::string& scenario_id, float debounce_seconds);

  std::string cm_per_360;
  std::string theme_name;
  std::string metronome_bpm;
  std::string crosshair_size;
  std::string crosshair_name;
  std::string dpi;
  bool disable_click_to_start = false;
  Keybinds keybinds;

 private:
  SettingsManager* settings_manager_;
  HistoryDb* history_db_;
  Stopwatch last_update_timer_;
};

}  // namespace aim

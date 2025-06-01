#pragma once

#include <SDL3/SDL.h>
#include <absl/status/status.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/common/times.h"
#include "aim/database/history_db.h"
#include "aim/database/settings_db.h"
#include "aim/proto/settings.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {

std::string GetMouseButtonName(u8 button);
std::string GetKeyNameForEvent(const SDL_Event& event);
bool KeyNameMatchesEvent(const SDL_Event& event, const std::string& name);
bool KeyMappingMatchesEvent(const SDL_Event& event, const KeyMapping& mapping);
bool KeyMappingMatchesEvent(const std::string& event_name, const KeyMapping& mapping);
bool IsMappableKeyDownEvent(const SDL_Event& event);
bool IsMappableKeyUpEvent(const SDL_Event& event);
bool IsEscapeKeyDown(const SDL_Event& event);

Theme GetDefaultTheme();
Crosshair GetDefaultCrosshair();
HealthBarAppearance GetDefaultHealthBarAppearance();

struct ThemeCacheEntry {
  Theme theme;
  std::filesystem::path file_path;
  std::filesystem::file_time_type last_modified_time;
};

class SettingsManager {
 public:
  explicit SettingsManager(const std::filesystem::path& settings_path,
                           const std::filesystem::path& theme_dir,
                           const std::filesystem::path& texture_dir,
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
  std::vector<std::string> ListTextures();
  void SaveThemeToDisk(const std::string& theme_name, const Theme& theme);

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
  std::filesystem::path theme_dir_;
  std::filesystem::path texture_dir_;
  std::unordered_map<std::string, ThemeCacheEntry> theme_cache_;
  std::unordered_map<std::string, ScenarioSettings> scenario_settings_cache_;
  SettingsDb* settings_db_;
  HistoryDb* history_db_;
};

struct SettingsUpdater {
 public:
  explicit SettingsUpdater(SettingsManager* settings_manager, HistoryDb* history_db);

  void SaveIfChangesMade(const std::string& scenario_id);

  Settings settings;

 private:
  SettingsManager* settings_manager_;
  HistoryDb* history_db_;
};

}  // namespace aim

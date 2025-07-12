#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "SDL3/SDL.h"
#include "absl/status/status.h"
#include "aim/common/simple_types.h"
#include "aim/common/times.h"
#include "aim/core/history_manager.h"
#include "aim/database/settings_db.h"
#include "aim/proto/crosshair.pb.h"
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

class SettingsManager;

struct SettingsUpdater {
 public:
  explicit SettingsUpdater(SettingsManager* settings_manager, HistoryManager* history_manager);

  void SaveIfChangesMade(const std::string& scenario_id);

  Settings settings;

 private:
  SettingsManager* settings_manager_;
  HistoryManager* history_manager_;
};

class SettingsManager {
 public:
  explicit SettingsManager(const std::filesystem::path& settings_path,
                           const std::filesystem::path& theme_dir,
                           const std::filesystem::path& texture_dir,
                           const std::filesystem::path& crosshair_dir,
                           SettingsDb* settings_db,
                           HistoryManager* history_manager);
  ~SettingsManager();
  AIM_NO_COPY(SettingsManager);

  absl::Status Initialize();

  float GetDpi();
  Settings GetCurrentSettings();
  Settings GetCurrentSettingsForScenario(const std::string& scenario_id);
  Settings* GetMutableCurrentSettings();

  Theme GetTheme(const std::string& theme_name);
  Theme GetThemeNoReferenceFollow(const std::string& theme_name);
  Theme GetCurrentTheme();
  bool ThemeExists(const std::string& name);
  bool SaveTheme(const std::string& name, const Theme& crosshair);
  bool DeleteTheme(const std::string& name);
  void RenameTheme(const std::string& old_name, const std::string& new_name);

  Crosshair GetCrosshair(const std::string& name);
  bool CrosshairExists(const std::string& name);
  bool SaveCrosshair(const std::string& name, const Crosshair& crosshair);
  bool DeleteCrosshair(const std::string& name);
  void RenameCrosshair(const std::string& old_name, const std::string& new_name);

  std::vector<std::string> ListCrosshairs();
  std::vector<std::string> ListThemes();
  std::vector<std::string> ListTextures();

  Crosshair GetCurrentCrosshair();

  void MarkDirty();
  void FlushToDisk(const std::string& scenario_id);
  // Only flush to disk if marked dirty.
  bool MaybeFlushToDisk(const std::string& scenario_id);

  void MaybeInvalidateThemeCache();

  SettingsUpdater CreateUpdater();

 private:
  void WriteScenarioSettings(const std::string& scenario_id);
  std::filesystem::path GetCrosshairPath(const std::string& name);
  std::filesystem::path GetThemePath(const std::string& name);

  std::filesystem::path settings_path_;
  Settings settings_;
  bool needs_save_ = false;
  std::filesystem::path theme_dir_;
  std::filesystem::path texture_dir_;
  std::filesystem::path crosshair_dir_;
  std::unordered_map<std::string, ThemeCacheEntry> theme_cache_;
  std::unordered_map<std::string, ScenarioSettings> scenario_settings_cache_;
  std::unordered_map<std::string, Crosshair> crosshair_cache_;
  SettingsDb* settings_db_;
  HistoryManager* history_manager_;
};

}  // namespace aim

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
#include "aim/database/aim_db.h"
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

  void SaveIfChangesMade(const std::string& scenario_name);

  Settings settings;

 private:
  SettingsManager* settings_manager_;
  HistoryManager* history_manager_;
};

class SettingsManager {
 public:
  virtual ~SettingsManager() {}

  virtual absl::Status Initialize() = 0;

  virtual float GetDpi() = 0;
  virtual Settings GetCurrentSettings() = 0;
  virtual Settings GetCurrentSettingsForScenario(const std::string& scenario_name) = 0;
  virtual Settings* GetMutableCurrentSettings() = 0;

  virtual Theme GetTheme(const std::string& theme_name) = 0;
  virtual Theme GetThemeNoReferenceFollow(const std::string& theme_name) = 0;
  virtual Theme GetCurrentTheme() = 0;
  virtual bool ThemeExists(const std::string& name) = 0;
  virtual bool SaveTheme(const std::string& name, const Theme& crosshair) = 0;
  virtual bool DeleteTheme(const std::string& name) = 0;
  virtual void RenameTheme(const std::string& old_name, const std::string& new_name) = 0;

  virtual Crosshair GetCrosshair(const std::string& name) = 0;
  virtual bool CrosshairExists(const std::string& name) = 0;
  virtual bool SaveCrosshair(const std::string& name, const Crosshair& crosshair) = 0;
  virtual bool DeleteCrosshair(const std::string& name) = 0;
  virtual void RenameCrosshair(const std::string& old_name, const std::string& new_name) = 0;

  virtual std::vector<std::string> ListCrosshairs() = 0;
  virtual std::vector<std::string> ListThemes() = 0;
  virtual std::vector<std::string> ListTextures() = 0;

  virtual Crosshair GetCurrentCrosshair() = 0;

  virtual void MarkDirty() = 0;
  virtual void FlushToDisk(const std::string& scenario_name) = 0;
  // Only flush to disk if marked dirty.
  virtual bool MaybeFlushToDisk(const std::string& scenario_name) = 0;

  virtual void MaybeInvalidateThemeCache() = 0;

  virtual void RenameScenario(const std::string& old_name, const std::string& new_name) = 0;

  virtual SettingsUpdater CreateUpdater() = 0;
};

std::unique_ptr<SettingsManager> CreateSettingsManager(const std::filesystem::path& settings_path,
                                                       const std::filesystem::path& theme_dir,
                                                       const std::filesystem::path& texture_dir,
                                                       const std::filesystem::path& crosshair_dir,
                                                       AimDb* db,
                                                       HistoryManager* history_manager);

}  // namespace aim

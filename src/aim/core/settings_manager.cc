#include "settings_manager.h"

#include <absl/status/status.h>
#include <google/protobuf/json/json.h>
#include <google/protobuf/util/json_util.h>
#include <nlohmann/json.h>

#include <fstream>

#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"
#include "aim/proto/settings.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {
namespace {
constexpr const float kDefaultDpi = 1600;

Theme GetDefaultTheme() {
  Theme t;
  t.set_name("default");
  *(t.mutable_crosshair()->mutable_color()) = ToStoredColor("#FFAC1C");
  *(t.mutable_crosshair()->mutable_outline_color()) = ToStoredColor("#000000");

  *t.mutable_front_appearance()->mutable_color() = ToStoredColor(0.7);
  *t.mutable_floor_appearance()->mutable_color() = ToStoredColor(0.6);
  *t.mutable_roof_appearance()->mutable_color() = ToStoredColor(0.6);
  *t.mutable_side_appearance()->mutable_color() = ToStoredColor(0.65);

  *t.mutable_target_color() = ToStoredColor(0);
  *t.mutable_ghost_target_color() = ToStoredColor(0.65);
  return t;
}

Crosshair GetDefaultCrosshair() {
  Crosshair crosshair;
  crosshair.set_size(3);
  *crosshair.mutable_color() = ToStoredRgb(254, 138, 24);
  crosshair.mutable_dot()->set_draw_outline(true);
  *(crosshair.mutable_dot()->mutable_outline_color()) = ToStoredRgb(0, 0, 0);
  return crosshair;
}

Settings GetDefaultSettings() {
  Settings settings;
  settings.set_name("default");
  settings.set_cm_per_360(45);
  *settings.mutable_crosshair() = GetDefaultCrosshair();
  return settings;
}

FullSettings GetDefaultFullSettings() {
  FullSettings full_settings;
  Settings default_settings = GetDefaultSettings();
  *full_settings.add_saved_settings() = default_settings;
  full_settings.set_current_settings(default_settings.name());
  return full_settings;
}

}  // namespace

SettingsManager::SettingsManager(const std::filesystem::path& settings_path,
                                 std::vector<std::filesystem::path> theme_dirs)
    : theme_dirs_(std::move(theme_dirs)), settings_path_(settings_path) {}

SettingsManager::~SettingsManager() {
  if (needs_save_) {
    FlushToDisk();
  }
}

absl::Status SettingsManager::Initialize() {
  // WriteJsonMessageToFile(theme_dirs_[0] / "default.json", GetDefaultTheme());

  auto maybe_content = ReadFileContentAsString(settings_path_);
  if (maybe_content.has_value()) {
    google::protobuf::json::ParseOptions opts;
    opts.ignore_unknown_fields = true;
    opts.case_insensitive_enum_parsing = true;
    std::string json = *maybe_content;
    return google::protobuf::util::JsonStringToMessage(json, &full_settings_, opts);
  }

  // Write initial settings to file.
  full_settings_ = GetDefaultFullSettings();
  FlushToDisk();
  return absl::OkStatus();
}

Theme SettingsManager::GetTheme(const std::string& theme_name) {
  if (theme_name.size() == 0) {
    return GetDefaultTheme();
  }
  auto it = theme_cache_.find(theme_name);
  if (it != theme_cache_.end()) {
    return it->second.theme;
  }

  for (auto& dir : theme_dirs_) {
    auto path = dir / std::format("{}.json", theme_name);
    if (std::filesystem::exists(path)) {
      Theme theme;
      if (ReadJsonMessageFromFile(path, &theme)) {
        ThemeCacheEntry entry;
        theme.set_name(theme_name);
        entry.theme = theme;
        entry.file_path = path;
        entry.last_modified_time = std::filesystem::last_write_time(path);
        theme_cache_[theme_name] = entry;
        return theme;
      }
    }
  }

  return GetDefaultTheme();
}

Theme SettingsManager::GetCurrentTheme() {
  Settings* settings = GetMutableCurrentSettings();
  if (settings == nullptr) {
    return GetDefaultTheme();
  }
  return GetTheme(settings->theme_name());
}

void SettingsManager::MaybeInvalidateThemeCache() {
  std::vector<std::string> entries_to_invalidate;
  for (auto& map_entry : theme_cache_) {
    auto& cache_entry = map_entry.second;
    auto last_modified_time = std::filesystem::last_write_time(cache_entry.file_path);
    if (last_modified_time > cache_entry.last_modified_time) {
      Logger::get()->info("Invalidate theme cache entry for {}", map_entry.first);
      entries_to_invalidate.push_back(map_entry.first);
    }
  }
  for (auto& theme_name : entries_to_invalidate) {
    theme_cache_.erase(theme_name);
  }
}

float SettingsManager::GetDpi() {
  float dpi = full_settings_.system_settings().dpi();
  return dpi > 0 ? dpi : kDefaultDpi;
}

FullSettings SettingsManager::GetFullSettings() {
  return full_settings_;
}

FullSettings* SettingsManager::GetMutableFullSettings() {
  return &full_settings_;
}

Settings SettingsManager::GetCurrentSettings() {
  Settings* existing_settings = GetMutableCurrentSettings();
  if (existing_settings != nullptr) {
    return *existing_settings;
  }
  Settings settings = GetDefaultSettings();
  settings.set_name(full_settings_.current_settings());
  return settings;
}

Settings* SettingsManager::GetMutableCurrentSettings() {
  for (int i = 0; i < full_settings_.saved_settings_size(); ++i) {
    Settings* settings = full_settings_.mutable_saved_settings(i);
    if (settings->name() == full_settings_.current_settings() ||
        full_settings_.current_settings().size() == 0) {
      return settings;
    }
  }
  return nullptr;
}

void SettingsManager::MarkDirty() {
  needs_save_ = true;
}

void SettingsManager::MaybeFlushToDisk() {
  if (needs_save_) {
    FlushToDisk();
  }
}
void SettingsManager::FlushToDisk() {
  if (WriteJsonMessageToFile(settings_path_, full_settings_)) {
    needs_save_ = false;
  }
}

}  // namespace aim

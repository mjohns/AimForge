#include "settings_manager.h"

#include <SDL3/SDL.h>
#include <absl/status/status.h>
#include <absl/strings/ascii.h>
#include <absl/strings/strip.h>
#include <google/protobuf/json/json.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/message_differencer.h>
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
  crosshair.set_name("default");
  crosshair.mutable_dot()->set_draw_outline(true);
  return crosshair;
}

Settings GetDefaultSettings() {
  Settings settings;
  settings.set_dpi(800);
  settings.set_cm_per_360(45);
  *settings.add_saved_crosshairs() = GetDefaultCrosshair();
  settings.set_current_crosshair_name("default");
  settings.set_crosshair_size(15);

  Keybinds* binds = settings.mutable_keybinds();
  binds->mutable_fire()->set_mapping1("Left Click");
  binds->mutable_restart_scenario()->set_mapping1("R");
  binds->mutable_next_scenario()->set_mapping1("Space");
  binds->mutable_quick_metronome()->set_mapping1("B");
  binds->mutable_adjust_crosshair_size()->set_mapping1("C");
  binds->mutable_quick_settings()->set_mapping1("S");

  return settings;
}

}  // namespace

std::string GetMouseButtonName(u8 button) {
  if (button == SDL_BUTTON_LEFT) {
    return "Left Click";
  }
  if (button == SDL_BUTTON_MIDDLE) {
    return "Middle Click";
  }
  if (button == SDL_BUTTON_RIGHT) {
    return "Right Click";
  }
  return "";
}

bool IsMappableKeyDownEvent(const SDL_Event& event) {
  return event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_KEY_DOWN;
}

bool IsMappableKeyUpEvent(const SDL_Event& event) {
  return event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_MOUSE_BUTTON_UP;
}

std::string GetKeyNameForEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    return GetMouseButtonName(event.button.button);
  }
  if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
    return SDL_GetKeyName(event.key.key);
  }
  return "";
}

bool IsEscapeKeyDown(const SDL_Event& event) {
  return event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE;
}

bool KeyNameMatchesEvent(const SDL_Event& event, const std::string& name) {
  if (name.size() == 0) {
    return false;
  }
  return absl::AsciiStrToLower(name) == absl::AsciiStrToLower(GetKeyNameForEvent(event));
}

bool KeyMappingMatchesEvent(const SDL_Event& event, const KeyMapping& mapping) {
  std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
  return KeyMappingMatchesEvent(event_name, mapping);
}

bool KeyMappingMatchesEvent(const std::string& event_name, const KeyMapping& mapping) {
  if (event_name.size() == 0) {
    return false;
  }
  if (absl::AsciiStrToLower(mapping.mapping1()) == event_name) {
    return true;
  }
  if (absl::AsciiStrToLower(mapping.mapping2()) == event_name) {
    return true;
  }
  if (absl::AsciiStrToLower(mapping.mapping3()) == event_name) {
    return true;
  }
  if (absl::AsciiStrToLower(mapping.mapping4()) == event_name) {
    return true;
  }
  return false;
}

SettingsManager::SettingsManager(const std::filesystem::path& settings_path,
                                 std::vector<std::filesystem::path> theme_dirs,
                                 SettingsDb* settings_db,
                                 HistoryDb* history_db)
    : theme_dirs_(std::move(theme_dirs)),
      settings_path_(settings_path),
      settings_db_(settings_db),
      history_db_(history_db) {}

SettingsManager::~SettingsManager() {
  if (needs_save_) {
    FlushToDisk("");
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
    return google::protobuf::util::JsonStringToMessage(json, &settings_, opts);
  }

  // Write initial settings to file.
  settings_ = GetDefaultSettings();
  FlushToDisk("");
  return absl::OkStatus();
}

std::vector<std::string> SettingsManager::ListThemes() {
  auto recent_themes = history_db_->GetRecentViews(RecentViewType::THEME, 20);

  std::vector<std::string> all_theme_names;
  for (auto dir : theme_dirs_) {
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      std::string filename = entry.path().filename().string();
      if (std::filesystem::is_regular_file(entry) && filename.ends_with(".json")) {
        std::string name(absl::StripSuffix(filename, ".json"));
        all_theme_names.push_back(name);
      }
    }
  }

  std::vector<std::string> theme_names;
  for (auto& recent_theme : recent_themes) {
    if (VectorContains(all_theme_names, recent_theme.id)) {
      theme_names.push_back(recent_theme.id);
    }
  }

  for (auto& theme_name : all_theme_names) {
    if (!VectorContains(theme_names, theme_name)) {
      theme_names.push_back(theme_name);
    }
  }
  return theme_names;
}

Theme SettingsManager::GetTheme(const std::string& theme_name) {
  std::string current_theme_name = theme_name;
  for (int i = 0; i < 20; ++i) {
    Theme theme = GetThemeNoReferenceFollow(current_theme_name);
    if (!theme.has_reference()) {
      return theme;
    }
    current_theme_name = theme.reference();
  }

  return GetDefaultTheme();
}

Theme SettingsManager::GetThemeNoReferenceFollow(const std::string& theme_name) {
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
  bool something_changed = false;
  for (auto& map_entry : theme_cache_) {
    auto& cache_entry = map_entry.second;
    auto last_modified_time = std::filesystem::last_write_time(cache_entry.file_path);
    if (last_modified_time > cache_entry.last_modified_time) {
      Logger::get()->info("Invalidate theme cache entry for {}", map_entry.first);
      something_changed = true;
    }
  }
  if (something_changed) {
    // Invalidate everything since references could not have changed while the theme they point to
    // changed.
    theme_cache_.clear();
  }
}

float SettingsManager::GetDpi() {
  float dpi = settings_.dpi();
  return dpi > 0 ? dpi : kDefaultDpi;
}

Settings SettingsManager::GetCurrentSettingsForScenario(const std::string& scenario_id) {
  auto it = scenario_settings_cache_.find(scenario_id);
  ScenarioSettings scenario_settings;
  if (it != scenario_settings_cache_.end()) {
    scenario_settings = it->second;
  } else {
    scenario_settings = settings_db_->GetScenarioSettings(scenario_id);
    scenario_settings_cache_[scenario_id] = scenario_settings;
  }

  if (scenario_settings.has_cm_per_360()) {
    settings_.set_cm_per_360(scenario_settings.cm_per_360());
  }
  if (scenario_settings.has_cm_per_360_jitter()) {
    settings_.set_cm_per_360_jitter(scenario_settings.cm_per_360_jitter());
  }
  if (scenario_settings.has_theme_name()) {
    settings_.set_theme_name(scenario_settings.theme_name());
  }
  if (scenario_settings.has_metronome_bpm()) {
    settings_.set_metronome_bpm(scenario_settings.metronome_bpm());
  }
  if (scenario_settings.has_crosshair_size()) {
    settings_.set_crosshair_size(scenario_settings.crosshair_size());
  }
  if (scenario_settings.has_crosshair_name()) {
    settings_.set_current_crosshair_name(scenario_settings.crosshair_name());
  }
  if (scenario_settings.has_auto_hold_tracking()) {
    settings_.set_auto_hold_tracking(scenario_settings.auto_hold_tracking());
  }
  return settings_;
}

Settings SettingsManager::GetCurrentSettings() {
  return settings_;
}

Settings* SettingsManager::GetMutableCurrentSettings() {
  return &settings_;
}

Crosshair SettingsManager::GetCurrentCrosshair() {
  Settings settings = GetCurrentSettings();
  for (const Crosshair& crosshair : settings.saved_crosshairs()) {
    if (crosshair.name() == settings.current_crosshair_name()) {
      return crosshair;
    }
  }
  return GetDefaultCrosshair();
}

void SettingsManager::MarkDirty() {
  needs_save_ = true;
}

bool SettingsManager::MaybeFlushToDisk(const std::string& scenario_id) {
  if (needs_save_) {
    FlushToDisk(scenario_id);
    return true;
  }
  return false;
}
void SettingsManager::FlushToDisk(const std::string& scenario_id) {
  if (scenario_id.size() > 0) {
    ScenarioSettings scenario_settings;
    scenario_settings.set_crosshair_size(settings_.crosshair_size());
    scenario_settings.set_crosshair_name(settings_.current_crosshair_name());
    scenario_settings.set_cm_per_360(settings_.cm_per_360());
    scenario_settings.set_cm_per_360_jitter(settings_.cm_per_360_jitter());
    scenario_settings.set_metronome_bpm(settings_.metronome_bpm());
    scenario_settings.set_theme_name(settings_.theme_name());
    scenario_settings.set_auto_hold_tracking(settings_.auto_hold_tracking());
    settings_db_->UpdateScenarioSettings(scenario_id, scenario_settings);
    scenario_settings_cache_[scenario_id] = scenario_settings;
  }
  if (WriteJsonMessageToFile(settings_path_, settings_)) {
    needs_save_ = false;
  }
}

SettingsUpdater::SettingsUpdater(SettingsManager* settings_manager, HistoryDb* history_db)
    : settings_manager_(settings_manager), history_db_(history_db) {
  auto current_settings = settings_manager_->GetMutableCurrentSettings();
  if (current_settings != nullptr) {
    cm_per_360 = MaybeIntToString(current_settings->cm_per_360());
    cm_per_360_jitter = MaybeIntToString(current_settings->cm_per_360_jitter());
    theme_name = current_settings->theme_name();
    metronome_bpm = MaybeIntToString(current_settings->metronome_bpm());
    dpi = MaybeIntToString(current_settings->dpi());
    crosshair_size = MaybeIntToString(current_settings->crosshair_size());
    crosshair_name = current_settings->current_crosshair_name();
    disable_click_to_start = current_settings->disable_click_to_start();
    auto_hold_tracking = current_settings->auto_hold_tracking();
    keybinds = current_settings->keybinds();
  }
}

void SettingsUpdater::SaveIfChangesMadeDebounced(const std::string& scenario_id,
                                                 float debounce_seconds) {
  if (last_update_timer_.IsRunning() && last_update_timer_.GetElapsedSeconds() < debounce_seconds) {
    return;
  }
  SaveIfChangesMade(scenario_id);
}

void SettingsUpdater::SaveIfChangesMade(const std::string& scenario_id) {
  auto current_settings = settings_manager_->GetMutableCurrentSettings();
  float new_cm_per_360 = ParseFloat(cm_per_360);
  if (new_cm_per_360 > 0) {
    current_settings->set_cm_per_360(new_cm_per_360);
    settings_manager_->MarkDirty();
  }
  float new_cm_per_360_jitter = cm_per_360_jitter.size() == 0 ? 0 : ParseFloat(cm_per_360_jitter);
  if (new_cm_per_360_jitter != current_settings->cm_per_360_jitter()) {
    current_settings->set_cm_per_360_jitter(new_cm_per_360_jitter);
    settings_manager_->MarkDirty();
  }
  if (current_settings->theme_name() != theme_name) {
    current_settings->set_theme_name(theme_name);
    settings_manager_->MarkDirty();
    if (theme_name.size() > 0) {
      history_db_->UpdateRecentView(RecentViewType::THEME, theme_name);
    }
  }
  if (current_settings->current_crosshair_name() != crosshair_name) {
    current_settings->set_current_crosshair_name(crosshair_name);
    settings_manager_->MarkDirty();
    if (crosshair_name.size() > 0) {
      history_db_->UpdateRecentView(RecentViewType::CROSSHAIR, crosshair_name);
    }
  }
  if (current_settings->disable_click_to_start() != disable_click_to_start) {
    current_settings->set_disable_click_to_start(disable_click_to_start);
    settings_manager_->MarkDirty();
  }
  if (current_settings->auto_hold_tracking() != auto_hold_tracking) {
    current_settings->set_auto_hold_tracking(auto_hold_tracking);
    settings_manager_->MarkDirty();
  }
  if (!google::protobuf::util::MessageDifferencer::Equivalent(current_settings->keybinds(),
                                                              keybinds)) {
    *current_settings->mutable_keybinds() = keybinds;
    settings_manager_->MarkDirty();
  }
  float new_metronome_bpm = ParseFloat(metronome_bpm);
  if (new_metronome_bpm >= 0 && current_settings->metronome_bpm() != new_metronome_bpm) {
    current_settings->set_metronome_bpm(new_metronome_bpm);
    settings_manager_->MarkDirty();
  }
  float new_dpi = ParseFloat(dpi);
  if (new_dpi >= 0 && current_settings->dpi() != new_dpi) {
    current_settings->set_dpi(new_dpi);
    settings_manager_->MarkDirty();
  }
  float new_crosshair_size = ParseFloat(crosshair_size);
  if (new_crosshair_size >= 0 && current_settings->crosshair_size() != new_crosshair_size) {
    current_settings->set_crosshair_size(new_crosshair_size);
    settings_manager_->MarkDirty();
  }
  settings_manager_->MaybeFlushToDisk(scenario_id);
  settings_manager_->MaybeInvalidateThemeCache();

  last_update_timer_ = Stopwatch();
  last_update_timer_.Start();
}

}  // namespace aim

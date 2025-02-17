#include "settings_manager.h"

#include <absl/status/status.h>
#include <google/protobuf/json/json.h>
#include <google/protobuf/util/json_util.h>
#include <nlohmann/json.h>

#include <fstream>

#include "aim/common/file_util.h"
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

Theme GetTextureTheme() {
  Theme t;
  t.set_name("texture");
  *(t.mutable_crosshair()->mutable_color()) = ToStoredColor("#FFAC1C");
  *(t.mutable_crosshair()->mutable_outline_color()) = ToStoredColor("#000000");

  WallAppearance wall;
  wall.mutable_texture()->set_texture_name("granite_herringbone.jpg");
  wall.mutable_texture()->set_scale(2);

  WallAppearance sides;
  sides.mutable_texture()->set_texture_name("basalt.jpg");
  sides.mutable_texture()->set_scale(2.5);

  *t.mutable_front_appearance() = wall;
  *t.mutable_floor_appearance() = sides;
  *t.mutable_roof_appearance() = sides;
  *t.mutable_side_appearance() = sides;

  t.mutable_front_appearance()->mutable_texture()->set_mix_percent(0.2);
  t.mutable_floor_appearance()->mutable_texture()->set_mix_percent(0.4);
  t.mutable_roof_appearance()->mutable_texture()->set_mix_percent(0.4);
  t.mutable_side_appearance()->mutable_texture()->set_mix_percent(0.2);

  *t.mutable_target_color() = ToStoredColor(0);
  *t.mutable_ghost_target_color() = ToStoredColor(0.65);
  return t;
}

Theme GetSolarizedLightTheme() {
  Theme t;
  t.set_name("solarized_light");
  auto base03 = ToStoredColor("#002b36");
  auto base02 = ToStoredColor("#073642");
  auto base01 = ToStoredColor("#586e75");
  auto base00 = ToStoredColor("#657b83");
  auto base0 = ToStoredColor("#839496");
  auto base1 = ToStoredColor("#93a1a1");
  auto base2 = ToStoredColor("#eee8d5");
  auto base3 = ToStoredColor("#FDF6E3");
  auto yellow = ToStoredColor("#b58900");
  auto orange = ToStoredColor("#cb4b16");
  auto red = ToStoredColor("#dc322f");
  auto magenta = ToStoredColor("#d33682");
  auto violet = ToStoredColor("#6c71c4");
  auto blue = ToStoredColor("#268bd2");
  auto cyan = ToStoredColor("#2aa198");
  auto green = ToStoredColor("#859900");

  *(t.mutable_crosshair()->mutable_color()) = ToStoredColor("#abdbe3");
  *(t.mutable_crosshair()->mutable_outline_color()) = blue;

  *t.mutable_front_appearance()->mutable_color() = base3;
  *t.mutable_side_appearance()->mutable_color() = base2;

  *t.mutable_floor_appearance()->mutable_color() = ToStoredColor("#d9d3bf");
  *t.mutable_roof_appearance()->mutable_color() = ToStoredColor("#d9d3bf");

  *t.mutable_target_color() = base03;
  *t.mutable_ghost_target_color() = base01;

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

SettingsManager::SettingsManager(const std::filesystem::path& settings_path)
    : settings_path_(settings_path) {}

SettingsManager::~SettingsManager() {
  if (needs_save_) {
    FlushToDisk();
  }
}

absl::Status SettingsManager::Initialize() {
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
  if (theme_name == "solarized_light") {
    return GetSolarizedLightTheme();
  }
  if (theme_name == "textured") {
    return GetTextureTheme();
  }
  return GetTextureTheme();
  // return GetDefaultTheme();
}

Theme SettingsManager::GetCurrentTheme() {
  Settings* settings = GetMutableCurrentSettings();
  if (settings == nullptr) {
    return GetDefaultTheme();
  }
  return GetTheme(settings->theme_name());
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
  std::string json_string;
  google::protobuf::json::PrintOptions opts;
  opts.add_whitespace = true;
  auto status = google::protobuf::util::MessageToJsonString(full_settings_, &json_string, opts);
  if (!status.ok()) {
    Logger::get()->error("Unable to serialize settings to json: {}", status.message());
    return;
  }
  int indent = 2;
  nlohmann::json json_data = nlohmann::json::parse(json_string);
  std::string formatted_json = json_data.dump(indent, ' ', true);

  std::ofstream outfile(settings_path_);
  if (!outfile.is_open()) {
    Logger::get()->error("Unable to open settings file for writing: {}", settings_path_.string());
  }
  outfile << formatted_json << std::endl;
  outfile.close();
  needs_save_ = false;
}

}  // namespace aim

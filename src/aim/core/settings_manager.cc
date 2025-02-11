#include "settings_manager.h"

#include <fstream>

#include "aim/common/util.h"
#include "aim/proto/settings.pb.h"

namespace aim {
namespace {
constexpr const float kDefaultDpi = 1600;

Crosshair GetDefaultCrosshair() {
  /*
  #include <google/protobuf/util/json_util.h>
  std::string json;
  google::protobuf::util::MessageToJsonString(s, &json);
  */
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
    : settings_path_(settings_path) {
  std::ifstream file(settings_path, std::ios::binary);
  if (std::filesystem::exists(settings_path) && file.is_open()) {
    full_settings_.ParseFromIstream(&file);
    file.close();
  } else {
    // Create initial settings.
    full_settings_ = GetDefaultFullSettings();
    FlushToDisk();
  }
}

SettingsManager::~SettingsManager() {
  if (needs_save_) {
    FlushToDisk();
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
  needs_save_ = false;

  std::ofstream outfile(settings_path_, std::ios::binary);
  full_settings_.SerializeToOstream(&outfile);
  outfile.close();
}

}  // namespace aim

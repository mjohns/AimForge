#include "settings_manager.h"

#include <flatbuffers/idl.h>
#include <flatbuffers/util.h>

#include <fstream>

#include "aim/common/util.h"

namespace aim {
namespace {
constexpr const float kDefaultDpi = 1600;

CrosshairT GetDefaultCrosshair() {
  auto dot = std::make_unique<DotCrosshairT>();
  dot->dot_color = ToStoredRgbPtr(254, 138, 24);
  dot->outline_color = ToStoredRgbPtr(0, 0, 0);
  dot->draw_outline = true;
  dot->dot_size = 3;

  CrosshairT crosshair;
  crosshair.dot = std::move(dot);
  return crosshair;
}

SettingsT GetDefaultSettings() {
  SettingsT settings;
  settings.name = "default";
  settings.cm_per_360 = 45;
  settings.crosshair = std::make_unique<CrosshairT>(GetDefaultCrosshair());
  return settings;
}

FullSettingsT GetDefaultFullSettings() {
  FullSettingsT full_settings;
  full_settings.system_settings = std::make_unique<SystemSettingsT>();
  auto settings = std::make_unique<SettingsT>(GetDefaultSettings());
  full_settings.current_settings = settings->name;
  full_settings.settings_list.push_back(std::move(settings));
  return full_settings;
}

}  // namespace

SettingsManager::SettingsManager(const std::filesystem::path& settings_path)
    : settings_path_(settings_path) {
  std::ifstream file(settings_path, std::ios::binary | std::ios::ate);
  if (std::filesystem::exists(settings_path)) {
    if (file.is_open()) {
      std::streamsize size = file.tellg();  // Get file size
      file.seekg(0, std::ios::beg);         // Go back to beginning

      std::vector<char> buffer(size);    // Create a buffer to hold the data
      if (size > 0) {                    // Check if the file is not empty
        file.read(buffer.data(), size);  // Read the data
      }
      file.close();

      const FullSettings* root = aim::GetFullSettings(buffer.data());
      root->UnPackTo(&full_settings_);
    }
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
  float dpi = full_settings_.system_settings->dpi;
  return dpi > 0 ? dpi : kDefaultDpi;
}

FullSettingsT SettingsManager::GetFullSettings() {
  return full_settings_;
}

FullSettingsT* SettingsManager::GetMutableFullSettings() {
  return &full_settings_;
}

SettingsT SettingsManager::GetCurrentSettings() {
  SettingsT* existing_settings = GetMutableCurrentSettings();
  if (existing_settings != nullptr) {
    return *existing_settings;
  }
  SettingsT settings = GetDefaultSettings();
  settings.name = full_settings_.current_settings;
  return settings;
}

SettingsT* SettingsManager::GetMutableCurrentSettings() {
  for (auto& settings : full_settings_.settings_list) {
    if (settings->name == full_settings_.current_settings ||
        full_settings_.current_settings.size() == 0) {
      return settings.get();
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

  flatbuffers::FlatBufferBuilder fbb;
  fbb.Finish(FullSettings::Pack(fbb, &full_settings_));
  std::ofstream outfile(settings_path_, std::ios::binary);
  outfile.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
  outfile.close();
}

}  // namespace aim

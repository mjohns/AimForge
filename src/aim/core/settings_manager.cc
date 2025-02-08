#include "settings_manager.h"

#include <flatbuffers/idl.h>
#include <flatbuffers/util.h>

#include <fstream>

#include "aim/common/util.h"

namespace aim {
namespace {

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
  settings.dpi = 1600;

  auto scenario_settings = std::make_unique<ScenarioSettingsT>();
  scenario_settings->cm_per_360 = 45;
  scenario_settings->crosshair = std::make_unique<CrosshairT>(GetDefaultCrosshair());

  settings.scenario_settings.push_back(std::move(scenario_settings));
  return settings;
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

      const Settings* root = aim::GetSettings(buffer.data());
      root->UnPackTo(&settings_);
    }
  } else {
    // Create initial settings.
    WriteSettings(GetDefaultSettings());
  }
}

const SettingsT& SettingsManager::GetSettings() {
    // TODO: make sure settings have reasonable defaults.
  return settings_;
}

ScenarioSettingsT SettingsManager::GetScenarioSettings(const std::string& scenario_id) {
  ScenarioSettingsT* default_settings = nullptr;
  for (auto& scenario_settings : settings_.scenario_settings) {
    if (scenario_settings->name.size() == 0) {
      default_settings = scenario_settings.get();
    }
    // Which settings for this scenario?
  }
  return default_settings != nullptr ? *default_settings : ScenarioSettingsT();
}

void SettingsManager::WriteSettings(const SettingsT& settings) {
  settings_ = settings;

  flatbuffers::FlatBufferBuilder fbb;
  fbb.Finish(Settings::Pack(fbb, &settings));
  std::ofstream outfile(settings_path_, std::ios::binary);
  outfile.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
  outfile.close();
}

}  // namespace aim

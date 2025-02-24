#include "scenario_manager.h"

#include <absl/strings/strip.h>

#include "aim/common/files.h"
#include "aim/common/log.h"
#include "aim/common/util.h"

namespace aim {
namespace {

std::vector<ScenarioItem> LoadScenarios(const std::filesystem::path& base_dir, bool is_user_dir) {
  if (!std::filesystem::exists(base_dir)) {
    return {};
  }
  std::vector<ScenarioItem> scenarios;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(base_dir)) {
    if (std::filesystem::is_regular_file(entry)) {
      std::string filename = entry.path().filename().string();
      if (!filename.ends_with(".json")) {
        continue;
      }
      ScenarioItem item;
      item.name = absl::StripSuffix(filename, ".json");
      item.is_user_scenario = is_user_dir;

      if (!ReadJsonMessageFromFile(entry.path(), &item.def)) {
        Logger::get()->warn("Unable to read scenario {}", entry.path().string());
        continue;
      }
      item.def.set_scenario_id(item.name);

      bool found_scenarios = false;
      for (const auto& part : entry.path()) {
        std::string dir_name = part.string();
        if (!found_scenarios) {
          if (dir_name == "scenarios") {
            found_scenarios = dir_name == "scenarios";
          }
          continue;
        }
        if (!dir_name.starts_with(item.name)) {
          item.path_parts.push_back(dir_name);
        }
      }

      scenarios.push_back(item);
    }
  }
  return scenarios;
}

}  // namespace

ScenarioManager::ScenarioManager(const std::filesystem::path& base_scenario_dir,
                                 const std::filesystem::path& user_scenario_dir)
    : base_scenario_dir_(base_scenario_dir), user_scenario_dir_(user_scenario_dir) {}

void ScenarioManager::LoadScenariosFromDisk() {
  scenarios_.clear();
  PushBackAll(&scenarios_, LoadScenarios(base_scenario_dir_, false));
  PushBackAll(&scenarios_, LoadScenarios(user_scenario_dir_, true));
  last_update_time_ = GetMostRecentUpdateTime(user_scenario_dir_, base_scenario_dir_);
}

void ScenarioManager::ReloadScenariosIfChanged() {
  auto new_update_time = GetMostRecentUpdateTime(user_scenario_dir_, base_scenario_dir_);
  if (!new_update_time.has_value()) {
    return;
  }
  if (!last_update_time_.has_value() || *new_update_time > *last_update_time_) {
    LoadScenariosFromDisk();
    return;
  }
}

}  // namespace aim

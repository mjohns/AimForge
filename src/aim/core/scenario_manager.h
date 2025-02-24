#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "aim/proto/scenario.pb.h"

namespace aim {

struct ScenarioItem {
  std::string name;
  std::vector<std::string> path_parts;
  ScenarioDef def;
  bool is_user_scenario = false;
};

class ScenarioManager {
 public:
  ScenarioManager(const std::filesystem::path& base_scenario_dir,
                  const std::filesystem::path& user_scenario_dir);

  void LoadScenariosFromDisk();
  void ReloadScenariosIfChanged();

  const std::vector<ScenarioItem>& scenarios() const {
    return scenarios_;
  }

 private:
  std::optional<std::filesystem::file_time_type> GetNewLastUpdateTime();
  
  std::vector<ScenarioItem> scenarios_;
  std::filesystem::path base_scenario_dir_;
  std::filesystem::path user_scenario_dir_;
  std::optional<std::filesystem::file_time_type> last_update_time_;
};

}  // namespace aim

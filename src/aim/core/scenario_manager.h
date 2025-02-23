#pragma once

#include <filesystem>
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

  const std::vector<ScenarioItem>& scenarios() const {
    return scenarios_;
  }

 private:
  std::vector<ScenarioItem> scenarios_;
  std::filesystem::path base_scenario_dir_;
  std::filesystem::path user_scenario_dir_;
};

}  // namespace aim

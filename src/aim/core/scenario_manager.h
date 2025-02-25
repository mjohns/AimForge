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

// Nodes to represent the scenarios as a tree that can be rendered by ImGui
struct ScenarioNode {
  // Either name or scenario will be specified. If scenario is set, this is a leaf node.
  std::string name;
  std::optional<ScenarioItem> scenario;
  std::vector<std::unique_ptr<ScenarioNode>> child_nodes;
};

class ScenarioManager {
 public:
  ScenarioManager(const std::filesystem::path& base_scenario_dir,
                  const std::filesystem::path& user_scenario_dir);

  void LoadScenariosFromDisk();
  void ReloadScenariosIfChanged();

  std::optional<ScenarioDef> GetScenario(const std::string& scenario_id);

  const std::vector<ScenarioItem>& scenarios() const {
    return scenarios_;
  }

  const std::vector<std::unique_ptr<ScenarioNode>>& scenario_nodes() const {
    return scenario_nodes_;
  }

 private:
  std::vector<ScenarioItem> scenarios_;
  std::vector<std::unique_ptr<ScenarioNode>> scenario_nodes_;
  std::filesystem::path base_scenario_dir_;
  std::filesystem::path user_scenario_dir_;
  std::optional<std::filesystem::file_time_type> last_update_time_;
};

}  // namespace aim

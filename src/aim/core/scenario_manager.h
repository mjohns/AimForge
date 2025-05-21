#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "aim/common/resource_name.h"
#include "aim/core/file_system.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct ScenarioItem {
  ResourceName name;
  ScenarioDef def;

  std::string id() const {
    return name.full_name();
  }
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
  explicit ScenarioManager(FileSystem* fs);

  void LoadScenariosFromDisk();

  std::optional<ScenarioItem> GetScenario(const std::string& scenario_id);

  const std::vector<ScenarioItem>& scenarios() const {
    return scenarios_;
  }

  const std::vector<std::unique_ptr<ScenarioNode>>& scenario_nodes() const {
    return scenario_nodes_;
  }

  bool SaveScenario(const ResourceName& name, const ScenarioDef& def);
  bool DeleteScenario(const ResourceName& name);
  bool RenameScenario(const ResourceName& old_name, const ResourceName& new_name);

 private:
  std::optional<ScenarioItem> GetScenario(const std::string& scenario_id, int depth);
  std::optional<ScenarioItem> GetScenarioNoReferenceFollow(const std::string& scenario_id);

  std::vector<ScenarioItem> scenarios_;
  std::vector<std::unique_ptr<ScenarioNode>> scenario_nodes_;
  FileSystem* fs_;
};

}  // namespace aim

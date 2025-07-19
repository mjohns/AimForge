#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aim/common/resource_name.h"
#include "aim/common/simple_types.h"
#include "aim/core/file_system.h"
#include "aim/core/screen.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct ScenarioItem {
  ResourceName name;
  ScenarioDef def;
  ScenarioDef unevaluated_def;

  bool has_invalid_reference = false;

  std::string id() const {
    return name.full_name();
  }
};

ScenarioDef ApplyScenarioOverrides(const ScenarioDef& original_def);

class ScenarioManager {
 public:
  explicit ScenarioManager(FileSystem* fs);
  AIM_NO_COPY(ScenarioManager);

  void LoadScenariosFromDisk();
  std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name);

  std::optional<ScenarioItem> GetScenario(const std::string& scenario_id);

  // Gets the scenario following any references and applying all overrides.
  std::optional<ScenarioItem> GetEvaluatedScenario(const std::string& scenario_id);

  std::optional<ScenarioItem> GetCurrentScenario() {
    return GetScenario(current_scenario_id_);
  }

  const std::string& GetCurrentScenarioId() {
    return current_scenario_id_;
  }

  void ClearCurrentScenario() {
    current_scenario_id_ = "";
    current_running_scenario_ = {};
  }

  void GenerateScenarioLevels(const std::string& starting_scenario_id,
                              const ScenarioOverrides& overrides,
                              int num_levels);

  bool SetCurrentScenario(const std::string& scenario_id) {
    if (scenario_id != current_scenario_id_) {
      current_running_scenario_ = {};
    }
    current_scenario_id_ = scenario_id;
    return GetScenario(scenario_id).has_value();
  }

  std::shared_ptr<std::vector<ScenarioItem>> scenarios() const {
    return scenarios_;
  }

  bool SaveScenario(const ResourceName& name, const ScenarioDef& def);
  // Return the name the scenario was saved with if successful.
  std::optional<ResourceName> SaveScenarioWithUniqueName(const ResourceName& name,
                                                         const ScenarioDef& def);
  bool DeleteScenario(const ResourceName& name);
  bool RenameScenario(const ResourceName& old_name, const ResourceName& new_name);

  void OpenFile(const ResourceName& name);

  bool has_running_scenario() const {
    return current_running_scenario_ != nullptr;
  }

  std::shared_ptr<Screen> GetCurrentRunningScenario() {
    return current_running_scenario_;
  }

  void SetCurrentRunningScenario(std::shared_ptr<Screen> scenario) {
    current_running_scenario_ = std::move(scenario);
  }

  void RegisterRenameListener(
      std::function<void(const std::string& old_name, const std::string& new_name)> listener) {
    scenario_rename_listeners_.push_back(std::move(listener));
  }

 private:
  std::optional<ScenarioItem> GetEvaluatedScenario(
      const std::string& scenario_id, std::unordered_set<std::string>* visited_scenario_names);
  void UpdateCachedScenario(const std::string& name, const ScenarioItem& new_item);

  std::shared_ptr<std::vector<ScenarioItem>> scenarios_;
  std::unordered_map<std::string, ScenarioItem> scenario_map_;

  FileSystem* fs_;
  std::shared_ptr<Screen> current_running_scenario_;

  std::string current_scenario_id_;

  std::vector<std::function<void(const std::string& old_name, const std::string& new_name)>>
      scenario_rename_listeners_;
};

}  // namespace aim

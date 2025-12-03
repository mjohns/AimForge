#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "aim/common/resource_name.h"
#include "aim/common/simple_types.h"
#include "aim/core/file_system.h"
#include "aim/core/screen.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct ScenarioItem {
  ResourceName name;
  std::string scenario_id;

  // References have not been evaluated.
  ScenarioDef unevaluated_def;

  std::optional<float> forced_cm_per_360{};
  std::optional<float> level{};

  const std::string& id() const {
    return scenario_id;
  }
};

class ScenarioManager {
 public:
  virtual ~ScenarioManager() {}

  virtual std::optional<ScenarioItem> GetScenario(const std::string& scenario_id) = 0;

  virtual std::optional<ScenarioDef> GetEvaluatedScenarioDef(const std::string& scenario_id) = 0;

  virtual std::shared_ptr<std::vector<std::string>> scenario_names() const = 0;

  virtual bool SaveScenario(const ResourceName& name, const ScenarioDef& def) = 0;

  // Return the name the scenario was saved with if successful.
  virtual std::optional<ResourceName> SaveScenarioWithUniqueName(const ResourceName& name,
                                                                 const ScenarioDef& def) = 0;
  virtual bool DeleteScenario(const ResourceName& name) = 0;

  virtual bool RenameScenario(const ResourceName& old_name, const ResourceName& new_name) = 0;

  virtual bool SetCurrentScenario(const std::string& scenario_id) = 0;

  std::optional<ScenarioItem> GetCurrentScenario() {
    return GetScenario(GetCurrentScenarioId());
  }

  virtual const std::string& GetCurrentScenarioId() = 0;

  virtual void ClearCurrentScenario() = 0;

  virtual void SetCurrentRunningScenario(std::shared_ptr<Screen> scenario) = 0;

  virtual std::shared_ptr<Screen> GetCurrentRunningScenario() = 0;

  virtual bool has_running_scenario() const = 0;

  virtual void LoadScenariosFromDisk() = 0;

  virtual std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name) = 0;

  virtual void OpenFile(const ResourceName& name) = 0;

  virtual void RegisterRenameListener(
      std::function<void(const std::string& old_name, const std::string& new_name)> listener) = 0;
};

std::unique_ptr<ScenarioManager> CreateScenarioManager(FileSystem* fs);

}  // namespace aim

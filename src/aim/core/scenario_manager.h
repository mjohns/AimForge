#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aim/common/name_util.h"
#include "aim/common/simple_types.h"
#include "aim/core/file_system.h"
#include "aim/core/screen.h"
#include "aim/proto/bundle.pb.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

struct ScenarioItem {
  std::string name;
  std::optional<NameInfo> name_info;

  // References have not been evaluated.
  ScenarioDef unevaluated_def;
};

class ScenarioManager {
 public:
  virtual ~ScenarioManager() {}

  virtual void StartReload() = 0;
  virtual void LoadScenariosFromBundle(const std::string& bundle_name,
                                       const BundleFile& bundle) = 0;
  virtual void FinishReload() = 0;
  virtual std::unordered_set<std::string> GetDirtyBundles() = 0;
  virtual void ClearDirtyBundles() = 0;

  virtual void AddScenariosForBundle(const std::string& bundle_name, BundleFile* bundle_file) = 0;

  virtual std::optional<ScenarioItem> GetScenario(const std::string& scenario_name) = 0;

  virtual std::optional<ScenarioDef> GetEvaluatedScenarioDef(const std::string& scenario_name) = 0;

  virtual std::shared_ptr<std::vector<std::string>> scenario_names() const = 0;

  virtual void UpdateScenario(const std::string& name, const ScenarioDef& def) = 0;

  // Returns the name the scenario was saved with
  virtual std::string SaveScenarioWithUniqueName(const std::string& name,
                                                 const ScenarioDef& def) = 0;
  virtual void DeleteScenario(const std::string& name) = 0;

  virtual bool RenameScenario(const std::string& old_name, const std::string& new_name) = 0;

  virtual bool SetCurrentScenario(const std::string& scenario_name) = 0;

  // Gets the list of scenarios that are directly referencing the provided scenario.
  virtual std::vector<std::string> GetReferencingScenarios(const std::string& scenario_name) = 0;

  std::optional<ScenarioItem> GetCurrentScenario() {
    return GetScenario(GetCurrentScenarioName());
  }

  virtual const std::string& GetCurrentScenarioName() = 0;

  virtual void ClearCurrentScenario() = 0;

  virtual void SetCurrentRunningScenario(std::shared_ptr<Screen> scenario) = 0;

  virtual std::shared_ptr<Screen> GetCurrentRunningScenario() = 0;

  virtual bool has_running_scenario() const = 0;

  virtual std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name) = 0;

  virtual void RegisterRenameListener(
      std::function<void(const std::string& old_name, const std::string& new_name)> listener) = 0;
};

std::unique_ptr<ScenarioManager> CreateScenarioManager();

}  // namespace aim

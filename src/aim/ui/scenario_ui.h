#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aim/common/simple_types.h"

namespace aim {

class Application;

struct ScenarioBrowserResult {
  std::string scenario_to_start;
  std::string scenario_to_edit;
  std::string scenario_to_edit_copy;
  bool reload_scenarios = false;

  std::string scenario_stats_to_view;
  i64 run_id;
};

class ScenarioBrowserComponent {
 public:
  virtual ~ScenarioBrowserComponent() {}

  // Returns whether to open an individual playlist.
  virtual void Show(ScenarioBrowserResult* result) = 0;

  virtual void Reload() = 0;
};

std::unique_ptr<ScenarioBrowserComponent> CreateScenarioBrowserComponent(const std::string& id,
                                                                         Application* app);

void DrawCurrentScenarioComponent(const std::string& id, Application& app);

}  // namespace aim

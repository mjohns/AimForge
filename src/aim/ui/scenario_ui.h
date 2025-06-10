#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aim/core/application.h"

namespace aim {

struct ScenarioBrowserResult {
  std::string scenario_to_start;
  std::string scenario_to_edit;
  std::string scenario_to_edit_copy;
  bool reload_scenarios = false;

  std::string scenario_stats_to_view;
  i64 run_id;
};

enum ScenarioBrowserType {
  FULL,
  RECENT,
};

class ScenarioBrowserComponent {
 public:
  virtual ~ScenarioBrowserComponent() {}

  // Returns whether to open an individual playlist.
  virtual void Show(ScenarioBrowserType type, ScenarioBrowserResult* result) = 0;

  virtual void Reload() = 0;
};

std::unique_ptr<ScenarioBrowserComponent> CreateScenarioBrowserComponent(Application* app);

}  // namespace aim

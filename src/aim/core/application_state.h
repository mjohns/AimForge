#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aim/core/screen.h"

namespace aim {

enum ScenarioRunOption {
  START_CURRENT,
  RESUME_CURRENT,
  PLAYLIST_NEXT,
};

class ApplicationState {
 public:
  ApplicationState() {}

  ApplicationState(const ApplicationState&) = delete;
  ApplicationState& operator=(const ApplicationState&) = delete;

  // std::string current_scenario_id;
  // std::string current_playlist_id;
  std::shared_ptr<Screen> current_running_scenario;
  std::optional<ScenarioRunOption> scenario_run_option;
};

}  // namespace aim
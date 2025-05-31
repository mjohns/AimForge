#pragma once

#include <memory>
#include <string>

#include "aim/core/application.h"

namespace aim {

class Scenario;
class UiScreen;

class AppState {
 public:
  explicit AppState(Application* app);
  ~AppState();

 private:
  std::string current_scenario_id_;
  std::string current_playlist_id_;
  std::unique_ptr<Scenario> current_running_scenario_;
  std::unique_ptr<UiScreen> next_screen_;
  Application* app_;
};

}  // namespace aim
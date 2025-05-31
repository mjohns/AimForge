#pragma once

#include <memory>
#include <optional>

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"

namespace aim {

enum class AppScreen {
  SETTINGS,
  THEMES,
  SCENARIOS,
  PLAYLISTS,
};

enum class ScenarioRunOption { NONE, RUN, RESUME };

class AppUi {
 public:
  AppUi() {}
  virtual ~AppUi() {}

  virtual void Run() = 0;
};

std::unique_ptr<AppUi> CreateAppUi(Application* app);

}  // namespace aim

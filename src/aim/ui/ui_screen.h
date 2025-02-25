#pragma once

#include <memory>
#include <optional>

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"

namespace aim {

enum class AppScreen {
  SETTINGS,
  SCENARIOS,
  PLAYLISTS,
  STATS,
  EXIT,
  CURRENT_SCENARIO,
  CURRENT_PLAYLIST
};
enum class ScenarioRunOption { NONE, RUN, RESUME };

class AppUi {
 public:
  AppUi() {}
  virtual ~AppUi() {}

  virtual void Run() = 0;
};

std::unique_ptr<AppUi> CreateAppUi(Application* app);

class UiScreen {
 public:
  UiScreen(Application* app) : app_(app) {}
  virtual ~UiScreen() {}

  NavigationEvent Run();

 protected:
  virtual std::optional<NavigationEvent> OnBeforeEventHandling() {
    return {};
  }

  virtual void OnEvent(const SDL_Event& event, bool user_is_typing) {}

  virtual std::optional<NavigationEvent> OnKeyDown(const SDL_Event& event, bool user_is_typing) {
    return {};
  }

  virtual std::optional<NavigationEvent> OnKeyUp(const SDL_Event& event, bool user_is_typing) {
    return {};
  }

  virtual void DrawScreen() = 0;

  virtual void Resume();

  Application* app_ = nullptr;
};

enum class SettingsScreenType { FULL, QUICK, QUICK_METRONOME, QUICK_CROSSHAIR };

std::unique_ptr<UiScreen> CreateSettingsScreen(Application* app, SettingsScreenType type);

}  // namespace aim

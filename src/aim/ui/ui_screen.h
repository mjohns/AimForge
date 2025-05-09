#pragma once

#include <memory>
#include <optional>

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"

namespace aim {

class UiScreen {
 public:
  UiScreen(Application* app) : app_(app) {}
  virtual ~UiScreen() {}

  NavigationEvent Run();

 protected:
  virtual void OnEvent(const SDL_Event& event, bool user_is_typing) {}
  virtual void OnKeyDown(const SDL_Event& event, bool user_is_typing) {}
  virtual void OnKeyUp(const SDL_Event& event, bool user_is_typing) {}

  virtual void DrawScreen() = 0;

  virtual void Resume();
  virtual void Render();

  void ScreenDone(NavigationEvent nav_event = NavigationEvent::Done()) {
    return_value_ = nav_event;
  }

  Application* app_ = nullptr;

 private:
  std::optional<NavigationEvent> return_value_;
};

}  // namespace aim

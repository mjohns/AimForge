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

  virtual std::optional<NavigationEvent> OnKeyDown(const SDL_Event& event, bool user_is_typing) {
    return {};
  }

  virtual std::optional<NavigationEvent> OnKeyUp(const SDL_Event& event, bool user_is_typing) {
    return {};
  }

  virtual void DrawScreen() = 0;

  virtual void Resume();

  void Done() {
    screen_done_ = true;
  }

  Application* app_ = nullptr;

 private:
  bool screen_done_ = false;
};

}  // namespace aim

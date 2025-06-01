#pragma once

#include <SDL3/SDL.h>

#include <memory>

namespace aim {

class Application;
class ApplicationState;

class Screen {
 public:
  Screen(Application* app);
  virtual ~Screen() {}

  void EnsureAttached();
  void EnsureDetached();
  void UpdateScreenStack();
  bool should_continue() const;

  virtual void OnEvent(const SDL_Event& event, bool user_is_typing) {}
  virtual void OnKeyDown(const SDL_Event& event, bool user_is_typing) {}
  virtual void OnKeyUp(const SDL_Event& event, bool user_is_typing) {}

  // Called before event processing.
  virtual void OnTickStart() {}
  // Called after events have been processed.
  virtual void OnTick() = 0;

  void PopSelf();
  void ReturnHome();
  void PushNextScreen(std::shared_ptr<Screen> next_screen);

  Application* app() {
    return &app_;
  }

  ApplicationState* state() {
    return &state_;
  }

 protected:
  virtual void OnAttach() {}
  virtual void OnDetach() {}

  Application& app_;
  ApplicationState& state_;

 private:
  std::shared_ptr<Screen> next_screen_;
  bool popped_self_ = false;
  bool is_attached_ = false;
  bool return_home_ = false;
};

}  // namespace aim
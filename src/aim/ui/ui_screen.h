#pragma once

#include <span>
#include <string>

#include "aim/core/screen.h"

namespace aim {

class Application;

class UiScreen : public Screen {
 public:
  UiScreen(Application& app) : Screen(app) {}
  virtual ~UiScreen() {}

 protected:
  virtual void DrawScreen() = 0;
  virtual void Render();
  virtual void OnAttachUi() {}
  virtual void OnDetachUi() {}

  virtual void OnEvent(const SDL_Event& event, bool user_is_typing) {}

  void OnEvents(std::span<SDL_Event> events) override;

  void OnTickStart() override;
  void OnTick() override;
  void OnAttach() override;
  void OnDetach() override;

  void HandleDefaultScenarioEvents(const SDL_Event& event,
                                   bool user_is_typing,
                                   const std::string& scenario_name);
};

}  // namespace aim

#pragma once

#include <memory>
#include <optional>

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"

namespace aim {

class UiScreen : public Screen {
 public:
  UiScreen(Application* app) : Screen(app) {}
  virtual ~UiScreen() {}

 protected:
  virtual void DrawScreen() = 0;
  virtual void Render();
  virtual void OnAttachUi() {}
  virtual void OnDetachUi() {}

  void OnTickStart() override;
  void OnTick() override;
  void OnAttach() override;
  void OnDetach() override;

  void HandleDefaultScenarioEvents(const SDL_Event& event,
                                   bool user_is_typing,
                                   const std::string& scenario_id);
};

}  // namespace aim

#include "screen.h"

#include "aim/common/log.h"
#include "aim/core/application.h"
#include "aim/core/application_state.h"

namespace aim {

Screen::Screen(Application* app) : app_(*app), state_(app->state()) {}

void Screen::PopSelf() {
  popped_self_ = true;
}
void Screen::ReturnHome() {
  return_home_ = true;
}

void Screen::PushNextScreen(std::shared_ptr<Screen> next_screen) {
  next_screen_ = std::move(next_screen);
}

void Screen::UpdateScreenStack() {
  if (return_home_) {
    int i = 0;
    while (!app_.is_on_home_screen()) {
      i++;
      if (i > 2000) {
        break;
      }
      app_.PopScreen();
    }
  } else if (popped_self_) {
    auto top = app_.PopScreen();
    assert(top);
    assert(top.get() == this);
  }

  popped_self_ = false;
  return_home_ = false;

  if (next_screen_) {
    app_.PushScreen(std::move(next_screen_));
    next_screen_ = nullptr;
  }
}

void Screen::EnsureAttached() {
  if (!is_attached_) {
    is_attached_ = true;
    OnAttach();
  }
}

void Screen::EnsureDetached() {
  if (is_attached_) {
    is_attached_ = false;
    OnDetach();
  }
}

bool Screen::should_continue() const {
  return !popped_self_ && !next_screen_;
}

}  // namespace aim
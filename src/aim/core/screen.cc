#include "screen.h"

#include <cassert>

#include "aim/core/application.h"

namespace aim {

bool IsQuitEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_QUIT) {
    return true;
  }
  // ctrl-q to quit
  return event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_Q &&
         (event.key.mod & SDL_KMOD_CTRL);
}

Screen::Screen(Application& app) : app_(app), state_(app.state()) {}

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
        assert(false && "Screen stack is too large");
        break;
      }
      app_.PopScreenInternal();
    }
  } else if (popped_self_) {
    auto top = app_.PopScreenInternal();
    assert(top);
    assert(top.get() == this);
  }

  popped_self_ = false;
  return_home_ = false;

  if (next_screen_) {
    app_.PushScreenInternal(std::move(next_screen_));
    next_screen_ = {};
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

bool Screen::ShouldContinue() const {
  return !popped_self_ && !next_screen_;
}

}  // namespace aim

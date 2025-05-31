#include "ui_screen.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {

void UiScreen::OnTickStart() {
  if (!app_.has_input_focus()) {
    SDL_Delay(250);
  }
}

void UiScreen::OnTick() {
  app_.NewImGuiFrame();
  if (DisableFullscreenWindow()) {
    DrawScreen();
  } else {
    if (app_.BeginFullscreenWindow()) {
      DrawScreen();
    }
    ImGui::End();
  }

  Render();
  SDL_Delay(6);
}

void UiScreen::Render() {
  app_.Render();
}

void UiScreen::OnAttach() {
  app_.EnableVsync();
  SDL_SetWindowRelativeMouseMode(app_.sdl_window(), false);
  OnAttachUi();
}

void UiScreen::OnDetach() {
  OnDetachUi();
}

}  // namespace aim

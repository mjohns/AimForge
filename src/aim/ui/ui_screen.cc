#include "ui_screen.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {

NavigationEvent UiScreen::Run() {
  Resume();

  while (true) {
    if (return_value_.has_value()) {
      return *return_value_;
    }
    if (!app_->has_input_focus()) {
      SDL_Delay(250);
    }
    OnTickStart();

    SDL_Event event;
    ImGuiIO& io = ImGui::GetIO();
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        throw ApplicationExitException();
      }
      OnEvent(event, io.WantTextInput);
      if (event.type == SDL_EVENT_KEY_DOWN) {
        OnKeyDown(event, io.WantTextInput);
      }
      if (event.type == SDL_EVENT_KEY_UP) {
        OnKeyUp(event, io.WantTextInput);
      }
    }
    if (return_value_.has_value()) {
      return *return_value_;
    }

    app_->StartFullscreenImguiFrame();
    DrawScreen();
    ImGui::End();

    Render();

    SDL_Delay(6);
  }

  return NavigationEvent::Done();
}

void UiScreen::Render() {
  app_->Render();
}

void UiScreen::Resume() {
  app_->EnableVsync();
  SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
}

}  // namespace aim

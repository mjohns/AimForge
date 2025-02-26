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
    if (!app_->has_input_focus()) {
      SDL_Delay(250);
    }

    {
      auto maybe_nav_event = OnBeforeEventHandling();
      if (maybe_nav_event.has_value()) {
        return *maybe_nav_event;
      }
    }

    SDL_Event event;
    ImGuiIO& io = ImGui::GetIO();
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        throw ApplicationExitException();
      }
      OnEvent(event, io.WantTextInput);
      if (event.type == SDL_EVENT_KEY_DOWN) {
        auto maybe_nav_event = OnKeyDown(event, io.WantTextInput);
        if (maybe_nav_event.has_value()) {
          return *maybe_nav_event;
        }
      }
      if (event.type == SDL_EVENT_KEY_UP) {
        auto maybe_nav_event = OnKeyUp(event, io.WantTextInput);
        if (maybe_nav_event.has_value()) {
          return *maybe_nav_event;
        }
      }
    }

    app_->StartFullscreenImguiFrame();
    DrawScreen();
    ImGui::End();
    if (app_->StartRender()) {
      app_->FinishRender();
    }
  }

  return NavigationEvent::Done();
}

void UiScreen::Resume() {
  SDL_GL_SetSwapInterval(1);  // Enable vsync
  SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
}

}  // namespace aim

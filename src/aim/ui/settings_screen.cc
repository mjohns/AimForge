#include "settings_screen.h"

#include <backends/imgui_impl_sdl3.h>

namespace aim {
namespace {
}  // namespace

void SettingsScreen::Run(Application* app) {
  SDL_GL_SetSwapInterval(1);  // Enable vsync
  SDL_SetWindowRelativeMouseMode(app->GetSdlWindow(), false);
  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return;
      }
    }

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();

    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    if (app->StartRender()) {
      app->FinishRender();
    }
  }
}

}  // namespace aim

#include "settings_screen.h"

#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>

#include "aim/common/util.h"
#include "aim/core/settings_manager.h"

namespace aim {
namespace {}  // namespace

NavigationEvent SettingsScreen::Run(Application* app) {
  SDL_GL_SetSwapInterval(1);  // Enable vsync
  SDL_SetWindowRelativeMouseMode(app->GetSdlWindow(), false);

  SettingsManager* mgr = app->GetSettingsManager();
  SettingsT* current_settings = mgr->GetMutableCurrentSettings();

  std::string cm_per_360 = MaybeIntToString(current_settings->cm_per_360);
  cm_per_360.reserve(20);

  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return NavigationEvent::Exit();
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_ESCAPE) {
          float new_cm_per_360 = ParseFloat(cm_per_360);
          if (new_cm_per_360 > 0) {
            current_settings->cm_per_360 = new_cm_per_360;
            mgr->MarkDirty();
          }
          mgr->MaybeFlushToDisk();
          return NavigationEvent::GoBack();
        }
      }
    }

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();
    ImGui::Columns(2, "SettingsColumns", false);  // 2 columns, no borders
    ImGui::Text("CM/360");
    ImGui::NextColumn();
    ImGui::InputText("##CM_PER_360", &cm_per_360, ImGuiInputTextFlags_CharsDecimal);
    ImGui::End();

    if (app->StartRender()) {
      app->FinishRender();
    }
  }

  return NavigationEvent::GoBack();
}

}  // namespace aim

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
  Settings* current_settings = mgr->GetMutableCurrentSettings();

  std::string cm_per_360 = MaybeIntToString(current_settings->cm_per_360());
  cm_per_360.reserve(20);
  const ScreenInfo& screen = app->GetScreenInfo();

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
            current_settings->set_cm_per_360(new_cm_per_360);
            mgr->MarkDirty();
          }
          mgr->MaybeFlushToDisk();
          return NavigationEvent::Done();
        }
        if (keycode == SDLK_H) {
          return NavigationEvent::GoHome();
        }
        if (keycode == SDLK_R) {
          return NavigationEvent::RestartLastScenario();
        }
      }
    }

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();

    float width = screen.width * 0.5;
    float height = screen.height * 0.9;

    float x_start = (screen.width - width) * 0.5;
    float y_start = (screen.height - height) * 0.5;

    // ImGui::SetCursorPos(ImVec2(x_start, y_start));

    ImGui::Spacing();
    ImGui::Indent(x_start);

    ImGui::Columns(2, "SettingsColumns", false);  // 2 columns, no borders
    ImGui::Text("CM/360");

    ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText("##CM_PER_360", &cm_per_360, ImGuiInputTextFlags_CharsDecimal);

    ImGui::End();

    if (app->StartRender()) {
      app->FinishRender();
    }
  }

  return NavigationEvent::Done();
}

}  // namespace aim

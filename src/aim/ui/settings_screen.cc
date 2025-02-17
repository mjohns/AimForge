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
  SDL_SetWindowRelativeMouseMode(app->sdl_window(), false);

  SettingsManager* mgr = app->settings_manager();
  Settings* current_settings = mgr->GetMutableCurrentSettings();

  std::string cm_per_360 = MaybeIntToString(current_settings->cm_per_360());
  cm_per_360.reserve(20);
  std::string theme_name = current_settings->theme_name();
  const ScreenInfo& screen = app->screen_info();

  while (true) {
    if (!app->has_input_focus()) {
      SDL_Delay(250);
    }
    SDL_Event event;
    ImGuiIO& io = ImGui::GetIO();
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
          if (current_settings->theme_name() != theme_name) {
            current_settings->set_theme_name(theme_name);
            mgr->MarkDirty();
          }
          mgr->MaybeFlushToDisk();
          return NavigationEvent::Done();
        }
        if (!io.WantTextInput) {
          if (keycode == SDLK_H) {
            return NavigationEvent::GoHome();
          }
          if (keycode == SDLK_R) {
            return NavigationEvent::RestartLastScenario();
          }
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

    ImGui::NextColumn();
    ImGui::Text("Theme");

    ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText("##THEME_NAME", &theme_name);

    ImGui::End();

    if (app->StartRender()) {
      app->FinishRender();
    }
  }

  return NavigationEvent::Done();
}

NavigationEvent QuickSettingsScreen::Run(Application* app) {
  SDL_GL_SetSwapInterval(1);  // Enable vsync
  SDL_SetWindowRelativeMouseMode(app->sdl_window(), false);

  SettingsManager* mgr = app->settings_manager();
  Settings* current_settings = mgr->GetMutableCurrentSettings();

  std::string cm_per_360 = MaybeIntToString(current_settings->cm_per_360());
  cm_per_360.reserve(20);
  const ScreenInfo& screen = app->screen_info();

  const std::string original_cm_per_360 = cm_per_360;

  std::vector<std::string> sens_list = {
      "15",
      "20",
      "25",
      "30",
      "35",
      "40",
      "45",
      "50",
      "55",
      "60",
      "65",
      "70",
  };

  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return NavigationEvent::Exit();
      }
      if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        if (event.wheel.y != 0) {
          float cm_per_360_val = ParseFloat(cm_per_360);
          cm_per_360 = std::format("{}", cm_per_360_val + event.wheel.y);
        }
      }
      if (event.type == SDL_EVENT_KEY_UP) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_S) {
          if (original_cm_per_360 != cm_per_360) {
            float new_cm_per_360 = ParseFloat(cm_per_360);
            if (new_cm_per_360 > 0) {
              current_settings->set_cm_per_360(new_cm_per_360);
              mgr->MarkDirty();
            }
          }
          mgr->MaybeFlushToDisk();
          return NavigationEvent::Done();
        }
      }
    }

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();

    float width = screen.width * 0.5;
    float center_gap = 10;

    float x_start = (screen.width - width) * 0.5;

    // ImGui::SetCursorPos(ImVec2(x_start, y_start));
    ImGui::SetCursorPos(ImVec2(0, screen.height * 0.3));

    ImVec2 button_sz = ImVec2((width - center_gap) / 2.0, 40);
    ImGui::Indent(x_start);
    for (int i = 20; i <= 70; i += 10) {
      std::string sens1 = std::format("{}", i);
      std::string sens2 = std::format("{}", i + 5);
      if (ImGui::Button(sens1.c_str(), button_sz)) {
        cm_per_360 = sens1;
      }
      ImGui::SameLine();
      // ImGui::SetCursorPos(ImVec2(x_start, y_start));
      if (ImGui::Button(sens2.c_str(), button_sz)) {
        cm_per_360 = sens2;
      }
    }

    // ImGui::SetCursorPos(ImVec2(x_start, y_start));

    ImGui::Spacing();
    ImGui::PushItemWidth(button_sz.x);
    ImGui::InputText("##CM_PER_360", &cm_per_360, ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopItemWidth();

    ImGui::End();

    if (app->StartRender()) {
      app->FinishRender();
    }
  }

  return NavigationEvent::Done();
}

}  // namespace aim

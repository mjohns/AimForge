#include "home_screen.h"

#include <backends/imgui_impl_sdl3.h>

#include <format>

#include "aim/scenario/static_scenario.h"

namespace aim {
namespace {}  // namespace

void HomeScreen::Run(Application* app) {
  bool needs_reset = true;

  std::optional<StaticScenarioParams> scenario_to_start;
  bool stop_scenario = false;
  int duration_seconds = 60;
  int cm_per_360 = 45;
  while (!stop_scenario) {
    if (needs_reset) {
      SDL_GL_SetSwapInterval(1);  // Enable vsync
      SDL_SetWindowRelativeMouseMode(app->GetSdlWindow(), false);
      needs_reset = false;
    }
    if (scenario_to_start) {
      aim::StaticScenario s(*scenario_to_start);
      s.Run(app);
      scenario_to_start = {};
      needs_reset = true;
      continue;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return;
      }
    }

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();

    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);

    ImGui::Text("duration:");
    ImGui::SameLine();
    if (ImGui::RadioButton("60s", duration_seconds == 60)) {
      duration_seconds = 60;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("30s", duration_seconds == 30)) {
      duration_seconds = 30;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("10s", duration_seconds == 10)) {
      duration_seconds = 10;
    }

    ImGui::Text("cm/360:");
    std::vector<int> sens_options = {30, 35, 40, 45, 50, 55, 60, 65, 70};
    for (int sens : sens_options) {
      std::string label = std::format("{}", sens);
      ImGui::SameLine();
      if (ImGui::RadioButton(label.c_str(), cm_per_360 == sens)) {
        cm_per_360 = sens;
      }
    }

    ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);
    if (ImGui::Button("Start 1w3ts", sz)) {
      StaticScenarioParams params;
      params.num_targets = 3;
      params.room_height = 150;
      params.room_width = 170;
      params.target_radius = 1.5;
      params.duration_seconds = duration_seconds;
      params.cm_per_360 = cm_per_360;
      scenario_to_start = params;
    }
    ImGui::End();

    ImVec4 clear_color = ImVec4(0.7f, 0.7f, 0.7f, 1.00f);
    if (app->StartRender(clear_color)) {
      app->FinishRender();
    }
  }
}

}  // namespace aim

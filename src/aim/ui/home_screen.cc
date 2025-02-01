#include "home_screen.h"

#include <backends/imgui_impl_sdl3.h>

#include "aim/scenario/static_scenario.h"

namespace aim {
namespace {}  // namespace

void HomeScreen::Run(Application* app) {
  bool needs_reset = true;

  std::optional<StaticScenarioParams> scenario_to_start;
  bool stop_scenario = false;
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

    float cm_per_360 = 45;
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);
    if (ImGui::Button("Start 1w3ts", sz)) {
      StaticScenarioParams params;
      params.num_targets = 3;
      params.room_height = 150;
      params.room_width = 170;
      params.target_radius = 1.5;
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

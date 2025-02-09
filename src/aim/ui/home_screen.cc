#include "home_screen.h"

#include <backends/imgui_impl_sdl3.h>

#include <format>

#include "aim/scenario/static_scenario.h"
#include "aim/ui/settings_screen.h"

namespace aim {
namespace {}  // namespace

void HomeScreen::Run(Application* app) {
  bool needs_reset = true;

  std::optional<StaticScenarioParams> scenario_to_start;
  bool stop_scenario = false;
  int duration_seconds = 60;
  bool open_settings = false;
  while (!stop_scenario) {
    if (needs_reset) {
      SDL_GL_SetSwapInterval(1);  // Enable vsync
      SDL_SetWindowRelativeMouseMode(app->GetSdlWindow(), false);
      needs_reset = false;
    }
    if (scenario_to_start) {
      aim::StaticScenario::RunScenario(*scenario_to_start, app);
      scenario_to_start = {};
      needs_reset = true;
      continue;
    }
    if (open_settings) {
      SettingsScreen settings_screen;
      settings_screen.Run(app);
      open_settings = false;
      continue;
    }

    StaticScenarioParams base_1w_params;
    base_1w_params.room_height = 150;
    base_1w_params.room_width = 170;
    base_1w_params.target_radius = 1.5;
    base_1w_params.duration_seconds = duration_seconds;

    base_1w_params.target_placement.min_distance = 20;
    TargetRegion circle_region;
    circle_region.percent_chance = 0.3;
    circle_region.x_circle_percent = 0.6;
    circle_region.y_circle_percent = 0.35;
    base_1w_params.target_placement.regions.push_back(circle_region);

    TargetRegion square_region;
    square_region.x_percent = 0.7;
    square_region.y_percent = 0.6;
    base_1w_params.target_placement.regions.push_back(square_region);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return;
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_ESCAPE) {
          return;
        }
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

    ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);
    if (ImGui::Button("Settings", sz)) {
      open_settings = true;
    }
    if (ImGui::Button("Start Nicky EZ PZ", sz)) {
      StaticScenarioParams params = base_1w_params;
      params.scenario_id = "nick_ez_pz";
      params.num_targets = 7;
      params.target_radius = 3;
      scenario_to_start = params;
    }
    if (ImGui::Button("Start 1w3ts", sz)) {
      StaticScenarioParams params = base_1w_params;
      params.scenario_id = "1w3ts_intermediate_s5";
      params.num_targets = 3;
      params.metronome_bpm = 130;
      scenario_to_start = params;
    }
    if (ImGui::Button("Start 1w3ts hard", sz)) {
      StaticScenarioParams params = base_1w_params;
      params.scenario_id = "1w3ts_intermediate_s5_hard";
      params.num_targets = 3;
      params.metronome_bpm = 125;
      params.target_radius = 0.9;
      params.remove_closest_target_on_miss = true;
      scenario_to_start = params;
    }
    if (ImGui::Button("Start 1w1ts", sz)) {
      StaticScenarioParams params = base_1w_params;
      params.scenario_id = "1w1ts_intermediate_s5";
      params.num_targets = 1;
      params.remove_closest_target_on_miss = true;
      scenario_to_start = params;
    }
    if (ImGui::Button("Start 1w3ts poke", sz)) {
      StaticScenarioParams params = base_1w_params;
      params.scenario_id = "1w3ts_intermediate_s5_poke";
      params.num_targets = 3;
      params.metronome_bpm = 130;
      params.is_poke_ball = true;
      scenario_to_start = params;
    }
    if (ImGui::Button("Start raw control", sz)) {
      StaticScenarioParams params;
      params.scenario_id = "raw_control";
      params.num_targets = 3;
      params.room_height = 150;
      params.room_width = 170;
      params.target_radius = 0.95;
      params.duration_seconds = duration_seconds;
      // params.metronome_bpm = 180;

      params.target_placement.min_distance = 3;
      TargetRegion circle_region;
      circle_region.x_circle_percent = 0.08;
      circle_region.y_circle_percent = 0.08;
      params.target_placement.regions.push_back(circle_region);

      scenario_to_start = params;
    }
    ImGui::End();

    if (app->StartRender()) {
      app->FinishRender();
    }
  }
}

}  // namespace aim

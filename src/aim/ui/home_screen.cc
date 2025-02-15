#include "home_screen.h"

#include <backends/imgui_impl_sdl3.h>

#include <format>

#include "aim/proto/scenario.pb.h"
#include "aim/scenario/static_scenario.h"
#include "aim/ui/settings_screen.h"

namespace aim {
namespace {}  // namespace

void HomeScreen::Run(Application* app) {
  bool needs_reset = true;

  std::optional<ScenarioDef> scenario_to_start;
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
      auto nav_event = aim::RunStaticScenario(*scenario_to_start, app);
      if (nav_event.IsExit()) {
        return;
      }
      scenario_to_start = {};
      needs_reset = true;
      continue;
    }
    if (open_settings) {
      SettingsScreen settings_screen;
      auto nav_event = settings_screen.Run(app);
      if (nav_event.IsExit()) {
        return;
      }
      open_settings = false;
      continue;
    }

    Room default_wall;
    default_wall.mutable_simple_room()->set_height(150);
    default_wall.mutable_simple_room()->set_width(170);
    *default_wall.mutable_camera_position() = ToStoredVec3(0, -100.0f, 0);

    Room circular_wall;
    circular_wall.mutable_circular_room()->set_height(100);
    circular_wall.mutable_circular_room()->set_radius(150);
    *circular_wall.mutable_camera_position() = ToStoredVec3(0, 50, 0);

    ScenarioDef base_static_def;
    base_static_def.set_duration_seconds(duration_seconds);
    *base_static_def.mutable_room() = default_wall;

    ScenarioDef base_1w_def = base_static_def;
    {
      StaticScenarioDef* static_def = base_1w_def.mutable_static_def();
      static_def->set_target_radius(1.5);

      TargetPlacementStrategy* strat = static_def->mutable_target_placement_strategy();
      strat->set_min_distance(20);

      TargetRegion* circle_region = strat->add_regions();
      circle_region->set_percent_chance(0.3);
      circle_region->mutable_oval()->mutable_x_diamter()->set_x_percent_value(0.6);
      circle_region->mutable_oval()->mutable_y_diamter()->set_y_percent_value(0.32);

      TargetRegion* square_region = strat->add_regions();
      square_region->set_percent_chance(1);
      square_region->mutable_rectangle()->mutable_x_length()->set_x_percent_value(0.65);
      square_region->mutable_rectangle()->mutable_y_length()->set_y_percent_value(0.55);
    }

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
    if (ImGui::Button("Start Circle", sz)) {
      ScenarioDef def = base_1w_def;
      def.set_scenario_id("circle_test");
      *def.mutable_room() = circular_wall;
      def.mutable_static_def()->set_num_targets(5);
      def.mutable_static_def()->set_target_radius(2.5);

      TargetPlacementStrategy* strat =
          def.mutable_static_def()->mutable_target_placement_strategy();
      strat->clear_regions();

      TargetRegion* region = strat->add_regions();
      region->mutable_rectangle()->mutable_x_length()->set_absolute_value(35);
      region->mutable_rectangle()->mutable_y_length()->set_y_percent_value(0.5);

      scenario_to_start = def;
    }
    if (ImGui::Button("Start Nicky EZ PZ", sz)) {
      ScenarioDef def = base_1w_def;
      def.set_scenario_id("nick_ez_pz");
      def.mutable_static_def()->set_num_targets(7);
      def.mutable_static_def()->set_target_radius(3);
      scenario_to_start = def;
    }
    if (ImGui::Button("Start 1w3ts intermediate", sz)) {
      ScenarioDef def = base_1w_def;
      def.set_scenario_id("1w3ts_intermediate_s5");
      def.mutable_static_def()->set_num_targets(3);
      scenario_to_start = def;
    }
    if (ImGui::Button("Start 1w2ts advanced", sz)) {
      ScenarioDef def = base_1w_def;
      def.set_scenario_id("1w2ts_advanced_s5");
      def.mutable_static_def()->set_num_targets(2);
      def.mutable_static_def()->set_target_radius(1.18);
      scenario_to_start = def;
    }
    if (ImGui::Button("Start 1w3ts hard", sz)) {
      ScenarioDef def = base_1w_def;
      def.set_scenario_id("1w3ts_intermediate_s5_hard");
      def.mutable_static_def()->set_num_targets(3);
      def.mutable_static_def()->set_target_radius(0.9);
      def.mutable_static_def()->set_remove_closest_target_on_miss(true);
      scenario_to_start = def;
    }
    if (ImGui::Button("Start 1w1ts", sz)) {
      ScenarioDef def = base_1w_def;
      def.set_scenario_id("1w1ts_intermediate_s5");
      def.mutable_static_def()->set_num_targets(1);
      def.mutable_static_def()->set_remove_closest_target_on_miss(true);
      scenario_to_start = def;
    }
    if (ImGui::Button("Start 1w1ts vertical alternate", sz)) {
      ScenarioDef def = base_1w_def;
      def.set_scenario_id("1w1ts_intermediate_s5_vertical_alternate");
      def.mutable_static_def()->set_num_targets(1);
      def.mutable_static_def()->set_remove_closest_target_on_miss(true);
      TargetPlacementStrategy* strat =
          def.mutable_static_def()->mutable_target_placement_strategy();
      strat->clear_regions();
      strat->set_alternate_regions(true);

      TargetRegion* top = strat->add_regions();
      top->mutable_rectangle()->mutable_x_length()->set_x_percent_value(0.3);
      top->mutable_rectangle()->mutable_y_length()->set_y_percent_value(0.1);
      top->mutable_y_offset()->set_y_percent_value(0.2);

      TargetRegion* bottom = strat->add_regions();
      bottom->mutable_rectangle()->mutable_x_length()->set_x_percent_value(0.3);
      bottom->mutable_rectangle()->mutable_y_length()->set_y_percent_value(0.1);
      bottom->mutable_y_offset()->set_y_percent_value(-0.2);
      scenario_to_start = def;
    }
    if (ImGui::Button("Start 1w3ts poke", sz)) {
      ScenarioDef def = base_1w_def;
      def.set_scenario_id("1w3ts_intermediate_s5_poke");
      def.mutable_static_def()->set_num_targets(3);
      def.mutable_static_def()->set_is_poke_ball(true);
      scenario_to_start = def;
    }
    if (ImGui::Button("Start raw control", sz)) {
      ScenarioDef def = base_1w_def;
      def.set_scenario_id("raw_control");
      def.clear_static_def();
      StaticScenarioDef* static_def = def.mutable_static_def();
      static_def->set_target_radius(0.95);
      static_def->set_num_targets(3);

      TargetPlacementStrategy* strat = static_def->mutable_target_placement_strategy();
      strat->set_min_distance(3);

      TargetRegion* circle_region = strat->add_regions();
      circle_region->mutable_oval()->mutable_x_diamter()->set_x_percent_value(0.08);
      circle_region->mutable_oval()->mutable_y_diamter()->set_y_percent_value(0.08);

      scenario_to_start = def;
    }
    ImGui::End();

    if (app->StartRender()) {
      app->FinishRender();
    }
  }
}

}  // namespace aim

#include <backends/imgui_impl_sdl3.h>

#include <format>

#include "aim/proto/scenario.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/ui/ui_screen.h"

namespace aim {
namespace {

std::vector<ScenarioDef> GetScenarios() {
  Room default_wall;
  default_wall.mutable_simple_room()->set_height(150);
  default_wall.mutable_simple_room()->set_width(170);
  *default_wall.mutable_camera_position() = ToStoredVec3(0, -100.0f, 0);

  Room circular_wall;
  circular_wall.mutable_circular_room()->set_height(150);
  circular_wall.mutable_circular_room()->set_radius(100);
  circular_wall.mutable_circular_room()->set_width(170);
  circular_wall.mutable_circular_room()->set_side_angle_degrees(30);
  *circular_wall.mutable_camera_position() = ToStoredVec3(0, 0, 0);

  Room flat_circular_wall;
  flat_circular_wall.mutable_circular_room()->set_height(150);
  flat_circular_wall.mutable_circular_room()->set_radius(300);
  flat_circular_wall.mutable_circular_room()->set_width(250);
  flat_circular_wall.mutable_circular_room()->set_side_angle_degrees(30);
  *flat_circular_wall.mutable_camera_position() = ToStoredVec3(0, 200, 0);

  circular_wall = flat_circular_wall;

  Room barrel_wall;
  barrel_wall.mutable_barrel_room()->set_radius(85);
  *barrel_wall.mutable_camera_position() = ToStoredVec3(0, -100.0, 0);

  ScenarioDef base_static_def;
  base_static_def.set_duration_seconds(60);
  *base_static_def.mutable_room() = default_wall;

  std::vector<ScenarioDef> scenarios;

  ScenarioDef base_1w_def = base_static_def;
  {
    StaticScenarioDef* static_def = base_1w_def.mutable_static_def();
    base_1w_def.mutable_target_def()->add_profiles()->set_target_radius(1.5);

    TargetPlacementStrategy* strat = static_def->mutable_target_placement_strategy();
    strat->set_min_distance(20);

    TargetRegion* circle_region = strat->add_regions();
    circle_region->set_percent_chance(0.3);
    circle_region->mutable_ellipse()->mutable_x_diameter()->set_x_percent_value(0.6);
    circle_region->mutable_ellipse()->mutable_y_diameter()->set_y_percent_value(0.32);

    TargetRegion* square_region = strat->add_regions();
    square_region->set_percent_chance(1);
    square_region->mutable_rectangle()->mutable_x_length()->set_x_percent_value(0.65);
    square_region->mutable_rectangle()->mutable_y_length()->set_y_percent_value(0.55);
  }

  TargetProfile default_centering_profile;
  default_centering_profile.set_target_radius(1.2);
  default_centering_profile.set_speed(45);

  {
    ScenarioDef def = base_static_def;
    def.set_scenario_id("barrel_bounce");
    def.set_display_name("Barrel Bounce");
    *def.mutable_room() = barrel_wall;
    def.mutable_room()->mutable_barrel_room()->set_radius(75);

    def.mutable_target_def()->set_num_targets(4);

    *def.mutable_barrel_def() = BarrelScenarioDef();

    {
      auto* t = def.mutable_target_def()->add_profiles();
      t->set_target_radius(2.8);
      t->set_speed(60);
    }
    {
      auto* t = def.mutable_target_def()->add_profiles();
      t->set_target_radius(2.2);
      t->set_speed(50);
      t->set_speed_jitter(3);
    }
    def.mutable_target_def()->mutable_target_order()->Add(0);
    def.mutable_target_def()->mutable_target_order()->Add(0);
    def.mutable_target_def()->mutable_target_order()->Add(1);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_static_def;
    def.set_scenario_id("centering_test");
    def.set_display_name("Centering");
    *def.mutable_target_def()->add_profiles() = default_centering_profile;
    *def.mutable_centering_def()->mutable_start_position() = ToStoredVec3(-60, -3, 0);
    *def.mutable_centering_def()->mutable_end_position() = ToStoredVec3(60, -3, 0);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_static_def;
    def.set_scenario_id("overhead_centering_test");
    def.set_display_name("Overhead Centering");
    *def.mutable_target_def()->add_profiles() = default_centering_profile;
    *def.mutable_centering_def()->mutable_start_position() = ToStoredVec3(-60, -3, 50);
    *def.mutable_centering_def()->mutable_end_position() = ToStoredVec3(60, -3, 50);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_static_def;
    def.set_scenario_id("vertical_centering_test");
    def.set_display_name("Vertical Centering");
    *def.mutable_target_def()->add_profiles() = default_centering_profile;
    *def.mutable_centering_def()->mutable_start_position() = ToStoredVec3(0, -3, 45);
    *def.mutable_centering_def()->mutable_end_position() = ToStoredVec3(0, -3, -45);
    scenarios.push_back(def);
  }
  float diagonal = 35;
  float neg_diagonal = -35;
  {
    ScenarioDef def = base_static_def;
    def.set_scenario_id("diagonal_centering_test_1");
    def.set_display_name("Diagonal Centering 1");
    *def.mutable_target_def()->add_profiles() = default_centering_profile;
    *def.mutable_centering_def()->mutable_start_position() =
        ToStoredVec3(neg_diagonal, -3, neg_diagonal);
    *def.mutable_centering_def()->mutable_end_position() = ToStoredVec3(diagonal, -3, diagonal);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_static_def;
    def.set_display_name("Diagonal Centering 2");
    def.set_scenario_id("diagonal_centering_test_2");
    *def.mutable_target_def()->add_profiles() = default_centering_profile;
    *def.mutable_centering_def()->mutable_start_position() =
        ToStoredVec3(neg_diagonal, -3, diagonal);
    *def.mutable_centering_def()->mutable_end_position() = ToStoredVec3(diagonal, -3, neg_diagonal);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_display_name("Circle Room");
    def.set_scenario_id("circle_test");
    *def.mutable_room() = circular_wall;
    def.mutable_target_def()->set_num_targets(6);
    def.mutable_target_def()->add_profiles()->set_target_radius(2.5);

    TargetPlacementStrategy* strat = def.mutable_static_def()->mutable_target_placement_strategy();
    *strat = TargetPlacementStrategy();

    strat->set_min_distance(30);

    TargetRegion* region = strat->add_regions();
    region->mutable_rectangle()->mutable_x_length()->set_x_percent_value(0.8);
    region->mutable_rectangle()->mutable_y_length()->set_y_percent_value(0.55);

    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_display_name("Nick EZ PZ");
    def.set_scenario_id("nick_ez_pz");
    def.mutable_target_def()->set_num_targets(7);
    def.mutable_target_def()->add_profiles()->set_target_radius(3);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_display_name("1w3ts");
    def.set_scenario_id("1w3ts_intermediate_s5");
    def.mutable_target_def()->set_num_targets(3);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_display_name("1w2ts advanced");
    def.set_scenario_id("1w2ts_advanced_s5");
    def.mutable_target_def()->set_num_targets(2);
    def.mutable_target_def()->add_profiles()->set_target_radius(1.18);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_scenario_id("1w3ts_intermediate_s5_hard");
    def.set_display_name("1w3ts hard");
    def.mutable_target_def()->set_num_targets(3);
    def.mutable_target_def()->add_profiles()->set_target_radius(1.05);
    def.mutable_target_def()->set_remove_closest_on_miss(true);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_display_name("1w3ts hard poke");
    def.set_scenario_id("1w3ts_intermediate_s5_hard_poke");
    def.mutable_target_def()->set_num_targets(3);
    def.mutable_target_def()->add_profiles()->set_target_radius(1.05);
    def.mutable_target_def()->set_remove_closest_on_miss(true);
    def.mutable_static_def()->set_is_poke_ball(true);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_scenario_id("1w1ts_intermediate_s5");
    def.set_display_name("1w1ts");
    def.mutable_target_def()->set_num_targets(1);
    def.mutable_target_def()->set_remove_closest_on_miss(true);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_scenario_id("1w1ts_intermediate_s5_fixed_dist");
    def.set_display_name("1w3ts fixed");
    def.mutable_target_def()->set_num_targets(3);
    def.mutable_static_def()->set_newest_target_is_ghost(true);
    def.mutable_target_def()->set_remove_closest_on_miss(true);
    def.mutable_static_def()
        ->mutable_target_placement_strategy()
        ->set_fixed_distance_from_last_target(20);
    def.mutable_static_def()->mutable_target_placement_strategy()->set_min_distance(18);
    scenarios.push_back(def);

    def.set_scenario_id("1w1ts_intermediate_s5_fixed_dist_poke");
    def.set_display_name("1w3ts fixed poke");
    def.mutable_static_def()->set_is_poke_ball(true);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_scenario_id("1w1ts_intermediate_s5_vertical_alternate");
    def.set_display_name("vertical alternate");
    def.mutable_target_def()->set_num_targets(2);
    def.mutable_target_def()->set_remove_closest_on_miss(true);
    def.mutable_static_def()->set_newest_target_is_ghost(true);
    TargetPlacementStrategy* strat = def.mutable_static_def()->mutable_target_placement_strategy();
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
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_scenario_id("1w3ts_intermediate_s5_poke");
    def.set_display_name("1w3ts poke");
    def.mutable_target_def()->set_num_targets(3);
    def.mutable_static_def()->set_is_poke_ball(true);
    scenarios.push_back(def);
  }
  {
    ScenarioDef def = base_1w_def;
    def.set_scenario_id("raw_control");
    def.set_display_name("RawControl");
    def.clear_static_def();
    StaticScenarioDef* static_def = def.mutable_static_def();
    def.mutable_target_def()->set_num_targets(3);
    def.mutable_target_def()->add_profiles()->set_target_radius(0.95);

    TargetPlacementStrategy* strat = static_def->mutable_target_placement_strategy();
    strat->set_min_distance(3);

    TargetRegion* circle_region = strat->add_regions();
    circle_region->mutable_ellipse()->mutable_x_diameter()->set_x_percent_value(0.075);
    circle_region->mutable_ellipse()->mutable_y_diameter()->set_y_percent_value(0.075);

    scenarios.push_back(def);
  }

  return scenarios;
}

bool ShouldExit(const NavigationEvent& e) {
  bool should_display_home = e.IsGoHome() || e.IsDone();
  return !should_display_home;
}

class HomeScreen : public UiScreen {
 public:
  explicit HomeScreen(Application* app) : UiScreen(app), scenarios_(GetScenarios()) {}

 protected:
  std::optional<NavigationEvent> OnBeforeEventHandling() override {
    if (scenario_to_start_) {
      app_->settings_manager()->MaybeInvalidateThemeCache();
      auto nav_event = aim::RunScenario(*scenario_to_start_, app_);
      if (ShouldExit(nav_event)) {
        return nav_event;
      }
      scenario_to_start_ = {};
      Resume();
    }
    if (open_settings_) {
      auto settings_screen = CreateSettingsScreen(app_, SettingsScreenType::FULL);
      auto nav_event = settings_screen->Run();
      if (ShouldExit(nav_event)) {
        return nav_event;
      }
      open_settings_ = false;
      Resume();
    }
    return {};
  }

  std::optional<NavigationEvent> OnKeyDown(const SDL_Event& event, bool user_is_typing) override {
    SDL_Keycode keycode = event.key.key;
    if (keycode == SDLK_ESCAPE) {
      return NavigationEvent::Exit();
    }
    return {};
  }

  void DrawScreen() override {
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);

    float default_centering_speed = 45;
    float default_centering_radius = 1.2;

    ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);
    if (ImGui::Button("Settings", sz)) {
      open_settings_ = true;
    }
    for (auto& def : scenarios_) {
      if (ImGui::Button(def.display_name().c_str(), sz)) {
        scenario_to_start_ = def;
      }
    }
  }

 private:
  std::optional<ScenarioDef> scenario_to_start_;
  bool open_settings_ = false;

  std::vector<ScenarioDef> scenarios_;
};

}  // namespace

std::unique_ptr<UiScreen> CreateHomeScreen(Application* app) {
  return std::make_unique<HomeScreen>(app);
}

}  // namespace aim

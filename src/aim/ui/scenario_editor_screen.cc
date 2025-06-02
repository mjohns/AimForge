#include "scenario_editor_screen.h"

#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <functional>
#include <optional>

#include "aim/common/files.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/resource_name.h"
#include "aim/common/util.h"
#include "aim/common/wall.h"
#include "aim/core/camera.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"
#include "aim/scenario/scenario.h"

namespace aim {
namespace {

void Line() {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
}

const std::vector<std::pair<Room::TypeCase, std::string>> kRoomTypes{
    {Room::kSimpleRoom, "Box"},
    {Room::kCylinderRoom, "Cylinder"},
    {Room::kBarrelRoom, "Barrel"},
};

const std::vector<std::pair<InOutDirection, std::string>> kInOutDirections{
    {InOutDirection::IN, "Towards center"},
    {InOutDirection::OUT, "Away from center"},
    {InOutDirection::RANDOM, "Random"},
};

const std::vector<std::pair<ShotType::TypeCase, std::string>> kShotTypes{
    {ShotType::kClickSingle, "Click"},
    {ShotType::kTrackingInvincible, "Tracking"},
    {ShotType::kTrackingKill, "Tracking kill"},
    {ShotType::kPoke, "Poke"},
    {ShotType::TYPE_NOT_SET, "Default"},
};

const std::vector<ScenarioDef::TypeCase> kSingleTargetTrackingTypes{
    ScenarioDef::kWallStrafeDef,
    ScenarioDef::kCenteringDef,
    ScenarioDef::kWallArcDef,
};

const std::vector<std::pair<ScenarioDef::TypeCase, std::string>> kScenarioTypes{
    {ScenarioDef::kStaticDef, "Static"},
    {ScenarioDef::kCenteringDef, "Centering"},
    {ScenarioDef::kWallStrafeDef, "Wall Strafe"},
    {ScenarioDef::kBarrelDef, "Barrel"},
    {ScenarioDef::kLinearDef, "Linear"},
    {ScenarioDef::kWallArcDef, "Wall Arc"},
};

const std::vector<std::pair<TargetRegion::TypeCase, std::string>> kRegionTypes{
    {TargetRegion::kRectangle, "Rectangle"},
    {TargetRegion::kCircle, "Circle"},
    {TargetRegion::kEllipse, "Ellipse"},
};

Room GetDefaultSimpleRoom() {
  Room r;
  r.mutable_simple_room()->set_height(130);
  r.mutable_simple_room()->set_width(150);
  *r.mutable_camera_position() = ToStoredVec3(0, -100, 0);
  return r;
}

Room GetDefaultCylinderRoom() {
  Room r;
  r.mutable_cylinder_room()->set_height(130);
  r.mutable_cylinder_room()->set_radius(100);
  r.mutable_cylinder_room()->set_width(150);
  return r;
}

Room GetDefaultBarrelRoom() {
  Room r;
  r.mutable_barrel_room()->set_radius(75);
  *r.mutable_camera_position() = ToStoredVec3(0, -100, 0);
  return r;
}

void VectorEditor(const std::string& id, StoredVec3* v, ImVec2 char_size) {
  float values[3];
  values[0] = v->x();
  values[1] = v->y();
  values[2] = v->z();

  ImGui::SetNextItemWidth(char_size.x * 20);
  ImGui::InputFloat3(std::format("###{}_vector_input", id).c_str(), values, "%.1f");

  v->set_x(values[0]);
  v->set_y(values[1]);
  v->set_z(values[2]);
}

class ScenarioEditorScreen : public UiScreen {
 public:
  explicit ScenarioEditorScreen(const ScenarioEditorOptions& opts, Application& app)
      : UiScreen(app), target_manager_(GetDefaultSimpleRoom()) {
    projection_ = GetPerspectiveTransformation(app_.screen_info());
    auto themes = app_.settings_manager().ListThemes();
    if (themes.size() > 0) {
      theme_ = app_.settings_manager().GetTheme(themes[0]);
    } else {
      theme_ = GetDefaultTheme();
    }
    settings_ = app_.settings_manager().GetCurrentSettings();
    *def_.mutable_room() = GetDefaultSimpleRoom();
    bundle_names_ = app_.file_system()->GetBundleNames();

    auto initial_scenario = app_.scenario_manager().GetScenario(opts.scenario_id);
    if (initial_scenario.has_value()) {
      def_ = initial_scenario->def;
      name_ = initial_scenario->name;
      if (opts.is_new_copy) {
        std::string final_name = MakeUniqueName(
            name_.relative_name() + " Copy",
            app_.scenario_manager().GetAllRelativeNamesInBundle(name_.bundle_name()));
        *name_.mutable_relative_name() = final_name;
      } else {
        original_name_ = initial_scenario->name;
      }
    }
  }

 protected:
  void DrawScreen() override {
    ImGui::IdGuard cid("ScenarioEditor");
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_size_ = char_size;
    char_x_ = char_size_.x;

    if (ImGui::Begin("Details")) {
      notification_popup_.Draw();
      ImGui::SimpleDropdown(
          "BundlePicker", name_.mutable_bundle_name(), bundle_names_, char_x_ * 11);

      ImGui::PushItemWidth(char_x_ * 7);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(char_x_ * 40);
      ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());

      float duration_seconds = FirstGreaterThanZero(def_.duration_seconds(), 60);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Duration");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##DurationSeconds", &duration_seconds, 15, 1, "%.0f");
      def_.set_duration_seconds(duration_seconds);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Score range");
      ImGui::SameLine();
      ImGui::InputFloat(ImGui::InputFloatParams("StartScore")
                            .set_zero_is_unset()
                            .set_step(0.1, 2)
                            .set_precision(2)
                            .set_width(char_x_ * 12),
                        PROTO_FLOAT_FIELD(ScenarioDef, &def_, start_score));
      ImGui::SameLine();
      ImGui::Text("to");
      ImGui::SameLine();
      ImGui::InputFloat(ImGui::InputFloatParams("EndScore")
                            .set_zero_is_unset()
                            .set_step(0.1, 2)
                            .set_precision(2)
                            .set_width(char_x_ * 12),
                        PROTO_FLOAT_FIELD(ScenarioDef, &def_, end_score));

      if (ImGui::Button("Play")) {
        PlayScenario();
      }
      ImGui::SameLine();
      if (ImGui::Button("View Json")) {
        SetErrorMessage(MessageToJson(def_, 6));
      }

      if (ImGui::Button("Save", ImVec2(char_x_ * 14, 0))) {
        if (SaveScenario()) {
          app_.scenario_manager().LoadScenariosFromDisk();
          PopSelf();
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        PopSelf();
      }
    }
    ImGui::End();

    if (ImGui::Begin("Scenario")) {
      if (ImGui::BeginTabBar("ScenarioTabs")) {
        if (ImGui::BeginTabItem("Definition")) {
          DrawScenarioTypeEditor(char_size);
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Targets")) {
          DrawTargetEditor(char_size);
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Room")) {
          DrawRoomEditor(char_size);
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Description")) {
          ImGui::InputTextMultiline("##DescriptionInput",
                                    def_.mutable_description(),
                                    ImGui::GetContentRegionAvail(),
                                    ImGuiInputTextFlags_AllowTabInput);
          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }
    }
    ImGui::End();

    notification_popup_.Draw();
  }

  // Returns whether the screen should close
  bool SaveScenario() {
    if (name_.bundle_name().size() == 0 || name_.relative_name().size() == 0) {
      SetErrorMessage("Missing scenario name");
      return false;
    }

    auto& mgr = app_.scenario_manager();

    bool is_new_file =
        !original_name_.has_value() || original_name_->full_name() != name_.full_name();
    if (is_new_file) {
      auto existing_scenario_with_name = mgr.GetScenario(name_.full_name());
      if (existing_scenario_with_name.has_value()) {
        SetErrorMessage(std::format("Scenario \"{}\" already exists", name_.full_name()));
        return false;
      }

      if (original_name_.has_value()) {
        if (!mgr.RenameScenario(*original_name_, name_)) {
          SetErrorMessage(std::format("Unable to rename \"{}\" to \"{}\".",
                                      original_name_->full_name(),
                                      name_.full_name()));
          return false;
        }
      }
    }

    if (!mgr.SaveScenario(name_, def_)) {
      SetErrorMessage(std::format("Unable to save scenario \"{}\".", name_.full_name()));
      return false;
    }

    app_.scenario_manager().SetCurrentScenario(name_.full_name());
    return true;
  }

  void SetErrorMessage(const std::string& msg) {
    notification_popup_.NotifyOpen(msg);
  }

  void DrawScenarioTypeEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("ScenarioTypeEditor");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scenario type");
    ImGui::SameLine();

    if (def_.type_case() == ScenarioDef::TYPE_NOT_SET) {
      def_.mutable_static_def();
    }

    auto scenario_type = def_.type_case();
    ImGui::SimpleTypeDropdown("ScenarioTypeDropdown", &scenario_type, kScenarioTypes, char_x_ * 15);

    if (VectorContains(kSingleTargetTrackingTypes, scenario_type)) {
      def_.mutable_shot_type()->set_tracking_invincible(true);
    } else {
      DrawShotTypeEditor(char_size);
    }

    Line();

    if (scenario_type == ScenarioDef::kStaticDef) {
      DrawStaticEditor();
    }
    if (scenario_type == ScenarioDef::kCenteringDef) {
      DrawCenteringEditor();
    }
    if (scenario_type == ScenarioDef::kWallStrafeDef) {
      DrawWallStrafeEditor();
    }
    if (scenario_type == ScenarioDef::kLinearDef) {
      DrawLinearEditor();
    }
    if (scenario_type == ScenarioDef::kBarrelDef) {
      DrawBarrelEditor();
    }
    if (scenario_type == ScenarioDef::kWallArcDef) {
      DrawWallArcEditor();
    }
  }

  void DrawWallArcEditor() {
    ImGui::IdGuard cid("WallArcEditor");
    WallArcScenarioDef& d = *def_.mutable_wall_arc_def();

    ImGui::InputFloat(ImGui::InputFloatParams("Duration")
                          .set_label("Duration seconds")
                          .set_step(0.1, 2)
                          .set_min(0.2)
                          .set_precision(1)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(WallArcScenarioDef, &d, duration));

    ImGui::InputJitteredFloat(ImGui::InputFloatParams("ControlHeight")
                                  .set_label("Control height")
                                  .set_step(0.1, 2)
                                  .set_precision(1)
                                  .set_default(1)
                                  .set_width(char_x_ * 10),
                              PROTO_JITTERED_FIELD(WallArcScenarioDef, &d, control_height));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "The arc is defined by a quadratic bezier curve where start is (0, 0) and end is (2, 0). "
        "The specified height is for the control point (1, height). See "
        "https://www.desmos.com/calculator/scz7zhonfw");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Width");
    ImGui::SameLine();
    DrawRegionLengthEditor("Width", /*default_to_x=*/true, d.mutable_width());
    ImGui::SameLine();
    ImGui::HelpMarker("The arc will be stretched over the specified width");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Height");
    ImGui::SameLine();
    DrawRegionLengthEditor("Height", /*default_to_x=*/true, d.mutable_height());

    ImGui::InputBool(ImGui::InputBoolParams("StartOnGround").set_label("Start on ground"),
                     PROTO_BOOL_FIELD(WallArcScenarioDef, &d, start_on_ground));
  }

  void DrawLinearEditor() {
    ImGui::IdGuard cid("LinearEditor");
    LinearScenarioDef& d = *def_.mutable_linear_def();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Angle");
    ImGui::SameLine();
    float angle = d.angle();
    float angle_jitter = d.angle_jitter();
    JitteredValueInput("AngleInput", &angle, &angle_jitter, 1, 3, "%.0f");
    d.set_angle(angle);
    d.set_angle_jitter(angle_jitter);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial direction");
    ImGui::SameLine();
    InOutDirection direction_type = d.has_direction() ? d.direction() : InOutDirection::IN;
    ImGui::SimpleTypeDropdown(
        "DirectionTypeDropdown", &direction_type, kInOutDirections, char_x_ * 20);
    d.set_direction(direction_type);

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial target location");
    ImGui::Indent();
    DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
    ImGui::Unindent();

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Override wall width");
    ImGui::SameLine();
    bool has_width = d.has_width();
    float width = FirstGreaterThanZero(d.width(), 100);
    OptionalInputFloat("WidthOverride", &has_width, &width, 10, 20, "%.0f");
    if (has_width) {
      d.set_width(width);
    } else {
      d.clear_width();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Override wall height");
    ImGui::SameLine();
    bool has_height = d.has_height();
    float height = FirstGreaterThanZero(d.height(), 100);
    OptionalInputFloat("HeightOverride", &has_height, &height, 10, 20, "%.0f");
    if (has_height) {
      d.set_height(height);
    } else {
      d.clear_height();
    }
  }

  void DrawBarrelEditor() {
    ImGui::IdGuard cid("BarrelEditor");
    BarrelScenarioDef& d = *def_.mutable_barrel_def();

    if (!def_.room().has_barrel_room()) {
      ImGui::Text("Must use barrel room");
      return;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Direction radius percent");
    ImGui::SameLine();
    int percent = d.direction_radius_percent() * 100;
    if (!d.has_direction_radius_percent()) {
      percent = 45;
    } else if (percent <= 0) {
      percent = 1;
    }
    ImGui::SetNextItemWidth(char_x_ * 10);
    ImGui::InputInt("##DirectionRadiusPercent", &percent, 5, 10);
    d.set_direction_radius_percent(percent / 100.0);

    if (!d.has_target_placement_strategy()) {
      d.mutable_target_placement_strategy()->set_min_distance(15);
      CircleTargetRegion* region =
          d.mutable_target_placement_strategy()->add_regions()->mutable_circle();
      region->mutable_diameter()->set_x_percent_value(0.92);
      region->mutable_inner_diameter()->set_x_percent_value(0.6);
    }

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial target location");
    ImGui::Indent();
    DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
    ImGui::Unindent();
  }

  void DrawWallStrafeEditor() {
    ImGui::IdGuard cid("WallStrafeEditor");
    WallStrafeScenarioDef& w = *def_.mutable_wall_strafe_def();
    if (!w.has_width()) {
      w.mutable_width()->set_x_percent_value(0.85);
    }
    if (!w.has_height()) {
      w.mutable_height()->set_y_percent_value(0.85);
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Strafing bounds");
    ImGui::Indent();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Width");
    ImGui::SameLine();
    DrawRegionLengthEditor("Width", /*default_to_x=*/true, w.mutable_width());
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Height");
    ImGui::SameLine();
    DrawRegionLengthEditor("Height", /*default_to_x=*/false, w.mutable_height());
    ImGui::Unindent();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Strafe at height");
    bool has_y = w.has_y();
    ImGui::SameLine();
    ImGui::Checkbox("##HasStrafeHeight", &has_y);
    if (has_y) {
      ImGui::Indent();
      DrawRegionLengthEditor(
          "Height##StrafeHeight", /*default_to_x=*/false, w.mutable_y(), /*is_point=*/true);
      ImGui::Unindent();
    } else {
      w.clear_y();
    }

    if (w.profiles_size() == 0) {
      w.add_profiles();
    }

    Line();

    ImGui::Text("Strafe profiles");
    ImGui::Indent();
    DrawProfileList("StrafeProfileList",
                    "Profile",
                    w.mutable_profile_order(),
                    w.mutable_profiles(),
                    std::bind_front(&ScenarioEditorScreen::DrawWallStrafeProfile, this));
    ImGui::Unindent();

    ImGui::Spacing();

    Line();

    float acceleration = w.acceleration();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Acceleration");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 10);
    ImGui::InputFloat("##AccelerationInput", &acceleration, 5, 20, "%.0f");
    if (acceleration <= 0) {
      w.clear_acceleration();
    } else {
      w.set_acceleration(acceleration);
    }
    ImGui::SameLine();
    ImGui::HelpMarker("The target will accelearte in and out of changes of direction");
  }

  void DrawWallStrafeProfile(WallStrafeProfile* p) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Min distance");
    ImGui::SameLine();
    DrawRegionLengthEditor("MinDistance", /*default_to_x=*/true, p->mutable_min_distance());

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Max distance");
    ImGui::SameLine();
    DrawRegionLengthEditor("MaxDistance", /*default_to_x=*/true, p->mutable_max_distance());

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Angle");
    ImGui::SameLine();
    float angle = p->angle();
    float angle_jitter = p->angle_jitter();
    JitteredValueInput("AngleInput", &angle, &angle_jitter, 1, 3, "%.0f");
    p->set_angle(angle);
    p->set_angle_jitter(angle_jitter);

    bool has_speed_override = p->has_speed_override();
    float speed_override = p->speed_override();
    if (!p->has_speed_override()) {
      // Default to a teleport like value.
      speed_override = 400;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Speed override");
    ImGui::SameLine();
    OptionalInputFloat("SpeedOverrideInput", &has_speed_override, &speed_override, 5, 30, "%.0f");
    if (has_speed_override && speed_override > 0) {
      p->set_speed_override(speed_override);
    } else {
      p->clear_speed_override();
    }

    bool is_pause = p->pause_at_end_chance() > 0;
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Pause at end");
    ImGui::SameLine();
    ImGui::Checkbox("##PauseCheck", &is_pause);
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Specify a probability that the target will stop for a certain duration at the end of this "
        "strafe before changing direction.");
    if (is_pause) {
      ImGui::Indent();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Percent chance");
      float chance = FirstGreaterThanZero(p->pause_at_end_chance() * 100, 50);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(char_x_ * 8);
      ImGui::InputFloat("##PauseChance", &chance, 5, 15, "%.0f");
      chance = glm::clamp(chance, 0.0f, 100.0f);
      p->set_pause_at_end_chance(chance / 100.0f);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Pause seconds");
      float pause_seconds = FirstGreaterThanZero(p->pause_seconds(), 0.5);
      float jitter = p->pause_seconds_jitter();
      ImGui::SameLine();
      JitteredValueInput("PauseSecondsInput", &pause_seconds, &jitter, 0.1, 1, "%.1f");
      p->set_pause_seconds(pause_seconds);
      p->set_pause_seconds_jitter(jitter);
    } else {
      p->clear_pause_seconds();
      p->clear_pause_seconds_jitter();
      p->clear_pause_at_end_chance();
    }
  }

  void DrawCenteringEditor() {
    ImGui::IdGuard cid("CenteringEditor");
    CenteringScenarioDef& c = *def_.mutable_centering_def();
    if (c.wall_points_size() > 2 || c.has_target_placement_strategy()) {
      ImGui::Text("Unsupported editable features");
      return;
    }

    // Ensure two wall points.
    while (c.wall_points_size() < 2) {
      c.add_wall_points();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Point 1");
    ImGui::Indent();
    DrawRegionVec2Editor("Point1", c.mutable_wall_points(0));
    ImGui::Unindent();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Point 2");
    ImGui::Indent();
    DrawRegionVec2Editor("Point2", c.mutable_wall_points(1));
    ImGui::Unindent();

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Orient pill");
    ImGui::SameLine();
    bool orient_pill = c.orient_pill();
    ImGui::Checkbox("##OrientPillCheck", &orient_pill);
    if (orient_pill) {
      c.set_orient_pill(true);
    } else {
      c.clear_orient_pill();
    }
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Orient the pill based on the start and end position. For a vertical centering "
        "scenario this would turn the pill horizontal.");
  }

  void DrawStaticEditor() {
    ImGui::IdGuard cid("StaticEditor");
    DrawTargetPlacementStrategyEditor(
        "Placement", def_.mutable_static_def()->mutable_target_placement_strategy());
  }

  void DrawTargetPlacementStrategyEditor(const std::string& id, TargetPlacementStrategy* s) {
    ImGui::IdGuard cid(id);
    if (s->regions_size() == 0) {
      s->add_regions();
    }

    ImGui::Text("Target locations");
    ImGui::Indent();
    DrawProfileList("RegionList",
                    "Region",
                    s->mutable_region_order(),
                    s->mutable_regions(),
                    std::bind_front(&ScenarioEditorScreen::DrawTargetRegion, this));
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Min distance");
    ImGui::SameLine();

    bool use_min = s->has_min_distance();
    float min_value = s->min_distance();
    if (!s->has_min_distance()) {
      min_value = 20;
    }
    OptionalInputFloat("MinDistanceInput", &use_min, &min_value, 1, 10, "%.0f");
    if (use_min) {
      s->set_min_distance(min_value);
    } else {
      s->clear_min_distance();
    }
    ImGui::SameLine();
    ImGui::HelpMarker("Minimum distance between targets.");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Fixed distance");
    ImGui::SameLine();
    bool use_fixed = s->has_fixed_distance_from_last_target();
    ImGui::Checkbox("##FixedDistanceCheck", &use_fixed);
    if (use_fixed) {
      float value = s->fixed_distance_from_last_target();
      if (!s->has_fixed_distance_from_last_target()) {
        value = 20;
      }
      float jitter = s->fixed_distance_jitter();
      ImGui::SameLine();
      JitteredValueInput("FixedDistanceInput", &value, &jitter, 1, 5, "%.0f");
      s->set_fixed_distance_from_last_target(value);
      s->set_fixed_distance_jitter(jitter);
    } else {
      s->clear_fixed_distance_from_last_target();
      s->clear_fixed_distance_jitter();
    }
    ImGui::SameLine();
    ImGui::HelpMarker(
        "New target will be placed at a fixed distance from the last target that was added.");
  }

  void DrawTargetRegion(TargetRegion* region) {
    if (region->type_case() == TargetRegion::TYPE_NOT_SET) {
      region->mutable_rectangle();
    }
    auto region_type = region->type_case();
    ImGui::SimpleTypeDropdown("RegionTypeDropdown", &region_type, kRegionTypes, char_x_ * 15);

    if (region_type == TargetRegion::kRectangle) {
      auto* t = region->mutable_rectangle();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Width");
      ImGui::SameLine();
      DrawRegionLengthEditor("Width", /*default_to_x=*/true, t->mutable_x_length());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Height");
      ImGui::SameLine();
      DrawRegionLengthEditor("Height", /*default_to_x=*/false, t->mutable_y_length());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Inner");
      ImGui::SameLine();
      bool use_inner = t->has_inner_x_length() || t->has_inner_y_length();
      ImGui::Checkbox("##InnerCheckbox", &use_inner);
      if (use_inner) {
        ImGui::IdGuard lid("InnerInputs");
        ImGui::Indent();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Width");
        ImGui::SameLine();
        DrawRegionLengthEditor("Width", /*default_to_x=*/true, t->mutable_inner_x_length());

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Height");
        ImGui::SameLine();
        DrawRegionLengthEditor("Height", /*default_to_x=*/false, t->mutable_inner_y_length());

        ImGui::Unindent();
      } else {
        t->clear_inner_x_length();
        t->clear_inner_y_length();
      }
    }

    if (region_type == TargetRegion::kCircle) {
      auto* t = region->mutable_circle();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Diameter");
      ImGui::SameLine();
      DrawRegionLengthEditor("Diameter", /*default_to_x=*/true, t->mutable_diameter());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Inner diameter");
      ImGui::SameLine();
      bool use_inner = t->has_inner_diameter();
      ImGui::Checkbox("##InnerCheckbox", &use_inner);
      if (use_inner) {
        ImGui::Indent();
        DrawRegionLengthEditor("InnerDiameter", /*default_to_x=*/true, t->mutable_inner_diameter());
        ImGui::Unindent();
      } else {
        t->clear_inner_diameter();
      }
    }

    if (region_type == TargetRegion::kEllipse) {
      auto* t = region->mutable_ellipse();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("X diameter");
      ImGui::SameLine();
      DrawRegionLengthEditor("XDiameter", /*default_to_x=*/true, t->mutable_x_diameter());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Y diameter");
      ImGui::SameLine();
      DrawRegionLengthEditor("YDiameter", /*default_to_x=*/false, t->mutable_y_diameter());
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Depth");
    ImGui::SameLine();
    float depth = region->depth();
    float depth_jitter = region->depth_jitter();
    JitteredValueInput("DepthInput", &depth, &depth_jitter, 1, 5, "%.0f");
    region->set_depth(depth);
    region->set_depth_jitter(depth_jitter);
    ImGui::SameLine();
    ImGui::HelpMarker(
        "The distance away from the wall towards the camera. The greater the value, the further it "
        "is from the wall.");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Offset");
    ImGui::SameLine();
    bool use_offsets = region->has_x_offset() || region->has_y_offset();
    ImGui::Checkbox("##OffsetsCheckbox", &use_offsets);
    if (use_offsets) {
      ImGui::Indent();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("X offset");
      ImGui::SameLine();
      DrawRegionLengthEditor(
          "XOffset", /*default_to_x=*/true, region->mutable_x_offset(), /*is_point=*/true);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Y offset");
      ImGui::SameLine();
      DrawRegionLengthEditor(
          "YOffset", /*default_to_x=*/false, region->mutable_y_offset(), /*is_point=*/true);
      ImGui::Unindent();
    } else {
      region->clear_x_offset();
      region->clear_y_offset();
    }
  }

  template <typename T, typename DrawFn>
  void DrawProfileList(const std::string& id,
                       const std::string& type_name,
                       google::protobuf::RepeatedField<int>* order_list,
                       google::protobuf::RepeatedPtrField<T>* profile_list,
                       DrawFn&& draw_profile_fn) {
    ImGui::IdGuard cid(id);

    std::string lower_type_name = absl::AsciiStrToLower(type_name);
    ImGui::AlignTextToFramePadding();
    ImGui::TextFmt("Explicit {} selection order", lower_type_name);
    bool use_order = order_list->size() > 0;
    ImGui::SameLine();
    ImGui::Checkbox("##UseOrder", &use_order);
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Specify the order in which profiles should be selected. 0, 1 means alternate between "
        "first and second profile");
    if (use_order) {
      ImGui::Indent();
      if (order_list->size() == 0) {
        order_list->Add(0);
      }
      int remove_at_i = -1;
      for (int i = 0; i < order_list->size(); ++i) {
        ImGui::IdGuard lid("Order", i);
        u32 number = order_list->at(i);
        u32 step = 1;
        ImGui::AlignTextToFramePadding();
        ImGui::Text(type_name);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(char_x_ * 8);
        ImGui::InputScalar("##OrderItemInput", ImGuiDataType_U32, &number, &step, nullptr, "%u");
        number = std::min<u32>(number, profile_list->size() - 1);
        order_list->Set(i, number);

        auto last_size = ImGui::GetItemRectSize();

        ImGui::SameLine();
        if (ImGui::Button(kIconCancel)) {
          remove_at_i = i;
        }
      }
      if (ImGui::Button("Add##Order")) {
        order_list->Add(0);
      }
      if (remove_at_i >= 0) {
        order_list->erase(order_list->begin() + remove_at_i);
      }
      ImGui::Unindent();
    } else {
      order_list->Clear();
    }

    bool use_weights = order_list->size() == 0 && profile_list->size() > 1;
    int remove_at_i = -1;
    int move_up_i = -1;
    int move_down_i = -1;
    int copy_i = -1;

    float total_weight = 0;
    for (int i = 0; i < profile_list->size(); ++i) {
      auto* p = &profile_list->at(i);
      total_weight += p->weight();
    }

    for (int i = 0; i < profile_list->size(); ++i) {
      ImGui::IdGuard lid(type_name, i);
      auto* p = &profile_list->at(i);
      ImGui::AlignTextToFramePadding();
      ImGui::TextFmt("{} #{}", type_name, i);
      const char* item_menu_id = "profile_item_menu";
      if (ImGui::BeginPopupContextItem(item_menu_id)) {
        if (ImGui::Selectable("Move up")) {
          move_up_i = i;
        }
        if (ImGui::Selectable("Move down")) {
          move_down_i = i;
        }
        if (ImGui::Selectable("Copy")) {
          copy_i = i;
        }
        if (ImGui::Selectable("Delete")) {
          remove_at_i = i;
        }
        ImGui::EndPopup();
      }
      ImGui::OpenPopupOnItemClick(item_menu_id, ImGuiPopupFlags_MouseButtonRight);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(char_x_ * 22);
      ImGui::InputText("##DescriptionInput", p->mutable_description());
      ImGui::SameLine();
      if (ImGui::Button(kIconMoreVert)) {
        ImGui::OpenPopup(item_menu_id);
      }

      ImGui::Indent();
      if (use_weights) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Selection weight");
        ImGui::SameLine();
        int weight = p->weight();
        if (!p->has_weight()) {
          weight = 1;
        }
        ImGui::SetNextItemWidth(char_x_ * 10);
        ImGui::InputInt("##WeightInput", &weight, 1, 5);
        p->set_weight(weight);

        if (total_weight > 0) {
          ImGui::SameLine();
          float weight_percent = (weight / total_weight) * 100;
          ImGui::TextDisabled("%.0f%%", weight_percent);
        }
      } else {
        p->clear_weight();
      }

      draw_profile_fn(&profile_list->at(i));
      ImGui::Unindent();
    }

    if (remove_at_i >= 0) {
      profile_list->erase(profile_list->begin() + remove_at_i);
    } else if (move_up_i > 0) {
      int i1 = move_up_i;
      int i2 = move_up_i - 1;
      std::swap((*profile_list)[i1], (*profile_list)[i2]);
    } else if (move_down_i >= 0) {
      int i1 = move_down_i;
      int i2 = move_down_i + 1;
      if (i2 < profile_list->size()) {
        std::swap((*profile_list)[i1], (*profile_list)[i2]);
      }
    } else if (copy_i >= 0) {
      *profile_list->Add() = (*profile_list)[copy_i];
    }

    if (ImGui::Button(std::format("Add {}", lower_type_name).c_str())) {
      profile_list->Add();
    }
  }

  void DrawRegionLengthEditor(const std::string& id,
                              bool default_to_x,
                              RegionLength* length,
                              bool is_point = false) {
    ImGui::IdGuard cid(id);
    bool is_x = length->type_case() == RegionLength::kXPercentValue;
    bool is_y = length->type_case() == RegionLength::kYPercentValue;
    bool is_percent = is_x || is_y;

    ImGui::SetNextItemWidth(char_x_ * 9);
    if (is_percent) {
      float value = is_x ? length->x_percent_value() : length->y_percent_value();
      value *= 100;
      if (!is_point && value <= 0) {
        value = 50;
      }
      ImGui::InputFloat("##PercentValue", &value, 1, 5, "%.0f");

      value /= 100.0;
      if (is_x) {
        length->set_x_percent_value(value);
      } else {
        length->set_y_percent_value(value);
      }
    } else {
      // Absolute value.
      float value = length->value();
      if (!is_point && value <= 0) {
        value = 50;
      }
      ImGui::InputFloat("##AbsoluteValue", &value, 1, 5, "%.0f");
      length->set_value(value);
    }

    ImGui::SameLine();
    ImGui::Text("as percent");
    ImGui::SameLine();
    ImGui::Checkbox("##UsePercent", &is_percent);

    if (is_percent) {
      ImGui::SameLine();
      ImGui::Text("of");
      ImGui::SameLine();
      if (!is_x && !is_y) {
        if (default_to_x) {
          is_x = true;
        } else {
          is_y = true;
        }
      }

      ImGui::PushItemWidth(char_x_ * 7);
      const char* x_str = "width";
      const char* y_str = "height";
      if (ImGui::BeginCombo("##XYCombo", is_x ? x_str : y_str, 0)) {
        if (ImGui::Selectable(x_str, is_x)) {
          is_x = true;
          is_y = false;
        }
        if (is_x) {
          ImGui::SetItemDefaultFocus();
        }

        if (ImGui::Selectable(y_str, is_y)) {
          is_y = true;
          is_x = false;
        }
        if (is_y) {
          ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      if (is_x && !length->has_x_percent_value()) {
        length->set_x_percent_value(length->y_percent_value());
      }
      if (is_y && !length->has_y_percent_value()) {
        length->set_y_percent_value(length->x_percent_value());
      }

      ImGui::PopItemWidth();

      ImGui::SameLine();
      float evaluated_length = Wall::ForRoom(def_.room()).GetRegionLength(*length);
      ImGui::TextDisabled(MaybeIntToString(evaluated_length, 1).c_str());
    } else {
      if (!length->has_value()) {
        length->set_value(Wall::ForRoom(def_.room()).GetRegionLength(*length));
      }
    }
  }

  void DrawRegionVec2Editor(const std::string& id, RegionVec2* v) {
    ImGui::IdGuard cid(id);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("x");
    ImGui::SameLine();
    DrawRegionLengthEditor("X" + id, /*default_to_x=*/true, v->mutable_x(), /*is_point=*/true);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("y");
    ImGui::SameLine();
    DrawRegionLengthEditor("Y" + id, /*default_to_x=*/false, v->mutable_y(), /*is_point=*/true);
  }

  void DrawShotTypeEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("ShotTypeEditor");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Shot type");
    ImGui::SameLine();

    auto type = def_.shot_type().type_case();
    if (ImGui::SimpleTypeDropdown("ShotTypeDropdown", &type, kShotTypes, char_x_ * 15)) {
      if (type == ShotType::kClickSingle) {
        def_.mutable_shot_type()->set_click_single(true);
      }
      if (type == ShotType::kTrackingInvincible) {
        def_.mutable_shot_type()->set_tracking_invincible(true);
      }
      if (type == ShotType::kTrackingKill) {
        def_.mutable_shot_type()->set_tracking_kill(true);
      }
      if (type == ShotType::kPoke) {
        def_.mutable_shot_type()->set_poke(true);
      }
    }
  }

  void DrawRoomEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("RoomEditor");
    Room& room = *def_.mutable_room();

    ImGuiComboFlags combo_flags = 0;
    if (room.type_case() == Room::TYPE_NOT_SET) {
      room = GetDefaultSimpleRoom();
    }

    auto type = room.type_case();
    if (ImGui::SimpleTypeDropdown("RoomTypeDropdown", &type, kRoomTypes, char_x_ * 15)) {
      if (type != room.type_case()) {
        if (type == Room::kSimpleRoom) {
          room = GetDefaultSimpleRoom();
        }
        if (type == Room::kCylinderRoom) {
          room = GetDefaultCylinderRoom();
        }
        if (type == Room::kBarrelRoom) {
          room = GetDefaultBarrelRoom();
        }
      }
    }

    if (room.type_case() == Room::kSimpleRoom) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Width");
      ImGui::SameLine();
      float width = room.simple_room().width();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomWidth", &width, 10, 1, "%.0f");
      room.mutable_simple_room()->set_width(width);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Height");
      ImGui::SameLine();
      float height = room.simple_room().height();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomHeight", &height, 10, 1, "%.0f");
      room.mutable_simple_room()->set_height(height);
    }

    if (room.type_case() == Room::kBarrelRoom) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Radius");
      ImGui::SameLine();
      float radius = room.barrel_room().radius();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomRadius", &radius, 5, 1, "%.0f");
      room.mutable_barrel_room()->set_radius(radius);
    }

    if (room.type_case() == Room::kCylinderRoom) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Height");
      ImGui::SameLine();
      float height = room.cylinder_room().height();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomHeight", &height, 10, 1, "%.0f");

      bool use_width_percent = room.cylinder_room().width_perimeter_percent() > 0;
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Width as percent of perimeter?");
      ImGui::SameLine();
      ImGui::Checkbox("##WidthPercentCheckbox", &use_width_percent);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Width");
      ImGui::SameLine();
      if (use_width_percent) {
        float width_percent =
            FirstGreaterThanZero(room.cylinder_room().width_perimeter_percent() * 100, 40);
        ImGui::SetNextItemWidth(char_x_ * 12);
        ImGui::InputFloat("##WidthPercent", &width_percent, 5, 1, "%.1f");
        room.mutable_cylinder_room()->set_width_perimeter_percent(width_percent / 100.0);
        room.mutable_cylinder_room()->clear_width();
      } else {
        float width = FirstGreaterThanZero(room.cylinder_room().width(), 100);
        ImGui::SetNextItemWidth(char_x_ * 12);
        ImGui::InputFloat("##Width", &width, 10, 1, "%.0f");
        room.mutable_cylinder_room()->set_width(width);
        room.mutable_cylinder_room()->clear_width_perimeter_percent();
      }

      room.mutable_cylinder_room()->set_height(height);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Radius");
      ImGui::SameLine();
      float radius = room.cylinder_room().radius();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomRadius", &radius, 10, 1, "%.0f");
      room.mutable_cylinder_room()->set_radius(radius);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Draw sides");
      ImGui::SameLine();
      bool has_sides = !room.cylinder_room().hide_sides();
      ImGui::Checkbox("##DrawSides", &has_sides);
      room.mutable_cylinder_room()->set_hide_sides(!has_sides);

      if (has_sides) {
        float side_angle = room.cylinder_room().side_angle_degrees();
        if (side_angle <= 0) {
          side_angle = 20;
        }
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Side angle degrees");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(char_x_ * 12);
        ImGui::InputFloat("##SideAngle", &side_angle, 1, 1, "%.0f");
        room.mutable_cylinder_room()->set_side_angle_degrees(side_angle);
      } else {
        room.mutable_cylinder_room()->clear_side_angle_degrees();
      }
    }

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Camera position");
    ImGui::SameLine();
    VectorEditor("CameraPositionVector", room.mutable_camera_position(), char_size);

    bool has_camera_up = room.has_camera_up();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Set camera up");
    ImGui::SameLine();
    ImGui::Checkbox("##CameraUp", &has_camera_up);
    if (has_camera_up) {
      ImGui::SameLine();
      if (IsZero(room.camera_up())) {
        room.mutable_camera_up()->set_z(1);
      }
      VectorEditor("CameraUpVector", room.mutable_camera_up(), char_size);
    } else {
      room.clear_camera_up();
    }
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Define up for the camera (usually the z axis). This allows you to rotate the entire "
        "scenario. (1, 0, 1) would be a 45 degree rotation.");

    bool has_camera_front = room.has_camera_front();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Set camera front");
    ImGui::SameLine();
    ImGui::Checkbox("##CameraFront", &has_camera_front);
    if (has_camera_front) {
      ImGui::SameLine();
      if (IsZero(room.camera_front())) {
        room.mutable_camera_front()->set_y(1);
      }
      VectorEditor("CameraFrontVector", room.mutable_camera_front(), char_size);
    } else {
      room.clear_camera_front();
    }
  }

  void DrawTargetEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("TargetEditor");
    TargetDef* t = def_.mutable_target_def();

    bool is_single_target_tracking = VectorContains(kSingleTargetTrackingTypes, def_.type_case());
    if (is_single_target_tracking) {
      t->set_num_targets(1);
      if (t->profiles_size() == 0) {
        t->add_profiles();
      }
      if (t->profiles_size() > 1) {
        TargetProfile first_profile = t->profiles(0);
        t->clear_profiles();
        *t->add_profiles() = first_profile;
      }

      DrawTargetProfile(t->mutable_profiles(0));
      return;
    }

    int num_targets = t->num_targets();
    if (num_targets <= 0) {
      num_targets = 1;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Number");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 8);
    ImGui::InputInt("##NumberEntry", &num_targets, 1, 1);
    t->set_num_targets(num_targets);

    if (t->profiles_size() == 0) {
      t->add_profiles();
    }

    Line();

    ImGui::Text("Target profiles");
    ImGui::Indent();
    DrawProfileList("ProfileList",
                    "Profile",
                    t->mutable_target_order(),
                    t->mutable_profiles(),
                    std::bind_front(&ScenarioEditorScreen::DrawTargetProfile, this));
    ImGui::Unindent();

    Line();

    ImGui::AlignTextToFramePadding();

    ImGui::Text("Remove closest target on miss");
    ImGui::SameLine();
    bool remove_closest = t->remove_closest_on_miss();
    ImGui::Checkbox("##RemoveClosest", &remove_closest);
    t->set_remove_closest_on_miss(remove_closest);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Newest target is ghost");
    ImGui::SameLine();
    bool is_ghost = t->newest_target_is_ghost();
    ImGui::Checkbox("##IsGhost", &is_ghost);
    t->set_newest_target_is_ghost(is_ghost);
    ImGui::SameLine();
    ImGui::HelpMarker("Ghost targets are unkillable and drawn in a different color.");

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("New target delay seconds");
    ImGui::SameLine();
    float new_target_delay = t->new_target_delay_seconds();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##NewTargetDelay", &new_target_delay, 0.1, 0.1, "%.2f");
    if (new_target_delay > 0) {
      t->set_new_target_delay_seconds(new_target_delay);
    } else {
      t->clear_new_target_delay_seconds();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Remove target after seconds");
    ImGui::SameLine();
    float remove_after = t->remove_target_after_seconds();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##RemoveAfterDelay", &remove_after, 0.1, 0.1, "%.2f");
    if (remove_after > 0) {
      t->set_remove_target_after_seconds(remove_after);
    } else {
      t->clear_remove_target_after_seconds();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Stagger initial targets seconds");
    ImGui::SameLine();
    float stagger = t->stagger_initial_targets_seconds();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##StaggerDelay", &stagger, 0.1, 0.1, "%.2f");
    if (stagger > 0) {
      t->set_stagger_initial_targets_seconds(stagger);
    } else {
      t->clear_stagger_initial_targets_seconds();
    }
  }

  void DrawTargetProfile(TargetProfile* profile) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Radius");
    ImGui::SameLine();
    float target_radius = profile->target_radius();
    if (target_radius <= 0) {
      target_radius = 2;
    }
    float radius_jitter = profile->target_radius_jitter();
    JitteredValueInput("TargetRadiusInput", &target_radius, &radius_jitter, 0.1, 0.3, "%.1f");
    profile->set_target_radius(target_radius);
    profile->set_target_radius_jitter(radius_jitter);

    // TODO: target_hit_radius

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Speed");
    ImGui::SameLine();
    float speed = profile->speed();
    float speed_jitter = profile->speed_jitter();
    JitteredValueInput("SpeedInput", &speed, &speed_jitter, 1, 10, "%.1f");
    if (speed > 0) {
      profile->set_speed(speed);
      profile->set_speed_jitter(speed_jitter);
    } else {
      profile->clear_speed();
      profile->clear_speed_jitter();
    }

    bool has_growth = profile->target_radius_growth_time_seconds() > 0;
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Pulse");
    ImGui::SameLine();
    ImGui::Checkbox("##PulseCheckbox", &has_growth);
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Target will grow to a certain size over some duration. If it is not killed by "
        "then, it will be removed.");
    if (has_growth) {
      ImGui::Indent();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Time seconds");
      ImGui::SameLine();
      float growth_time = FirstGreaterThanZero(profile->target_radius_growth_time_seconds(), 2);
      ImGui::SetNextItemWidth(char_x_ * 10);
      ImGui::InputFloat("##GrowthTime", &growth_time, 0.1, 0.5, "%.1f");
      profile->set_target_radius_growth_time_seconds(std::max(growth_time, 0.1f));

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Final radius");
      ImGui::SameLine();
      float final_radius =
          FirstGreaterThanZero(profile->target_radius_growth_size(), profile->target_radius() * 3);
      ImGui::SetNextItemWidth(char_x_ * 10);
      ImGui::InputFloat("##FinalGrowthRadius", &final_radius, 0.1, 0.5, "%.1f");
      profile->set_target_radius_growth_size(std::max(final_radius, 0.1f));
      ImGui::Unindent();
    } else {
      profile->clear_target_radius_growth_time_seconds();
      profile->clear_target_radius_growth_size();
    }

    float health_seconds = profile->health_seconds();
    float health_seconds_jitter = profile->health_seconds_jitter();
    if (def_.shot_type().type_case() == ShotType::kTrackingKill) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Health");
      ImGui::SameLine();
      JitteredValueInput(
          "HealthSecondsInput", &health_seconds, &health_seconds_jitter, 0.1, 0.5, "%.1f");
      ImGui::SameLine();
      ImGui::HelpMarker("The amount of time in seconds to kill the target.");
    } else {
      health_seconds = 0;
    }
    if (health_seconds > 0) {
      profile->set_health_seconds(health_seconds);
      profile->set_health_seconds_jitter(health_seconds_jitter);
    } else {
      profile->clear_health_seconds();
      profile->clear_health_seconds_jitter();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Pill");
    ImGui::SameLine();
    bool use_pill = profile->has_pill();
    ImGui::Checkbox("##UsePill", &use_pill);
    if (use_pill) {
      ImGui::Indent();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Height");
      ImGui::SameLine();
      float height = profile->pill().height();
      if (height <= 0) {
        height = 20;
      }
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##PillHeightEntry", &height, 0.1, 1, "%.1f");
      profile->mutable_pill()->set_height(height);

      ImGui::Unindent();
    } else {
      profile->clear_pill();
    }
  }

  void JitteredValueInput(const std::string& id,
                          float* value,
                          float* jitter_value,
                          float step,
                          float fast_step,
                          const char* format = "%.1f") {
    ImGui::IdGuard cid(id);
    ImGui::SetNextItemWidth(char_x_ * 9);
    ImGui::InputFloat("##ValueEntry", value, step, fast_step, format);

    ImGui::SameLine();
    ImGui::Text("+/-");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 9);
    ImGui::InputFloat("##JitterEntry", jitter_value, step, fast_step, format);
    if (*jitter_value < 0) {
      *jitter_value = 0;
    }
  }

  void OptionalInputFloat(const std::string& id,
                          bool* has_value,
                          float* value,
                          float step,
                          float fast_step,
                          const char* format = "%.1f") {
    ImGui::OptionalInputFloat(id, has_value, value, step, fast_step, format, char_x_ * 12);
  }

  void Render() override {
    target_manager_.UpdateRoom(def_.room());
    CameraParams camera_params(def_.room());
    Camera camera(camera_params);
    auto look_at = camera.GetLookAt();

    RenderContext ctx;
    Stopwatch stopwatch;
    FrameTimes frame_times;
    if (app_.StartRender(&ctx)) {
      app_.renderer()->DrawScenario(projection_,
                                    def_.room(),
                                    theme_,
                                    settings_.health_bar(),
                                    target_manager_.GetTargets(),
                                    look_at,
                                    &ctx,
                                    stopwatch,
                                    &frame_times);
      app_.FinishRender(&ctx);
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (user_is_typing) {
      return;
    }
    if (IsMappableKeyDownEvent(event)) {
      std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
      bool is_restart = KeyMappingMatchesEvent(
          event_name, app_.settings_manager().GetCurrentSettings().keybinds().restart_scenario());
      bool is_next = KeyMappingMatchesEvent(
          event_name, app_.settings_manager().GetCurrentSettings().keybinds().next_scenario());
      if (is_restart || is_next) {
        PlayScenario();
      }
    }
  }

 private:
  void PlayScenario() {
    CreateScenarioParams params;
    params.def = def_;
    params.def.set_duration_seconds(1000000);
    params.id = name_.full_name();
    params.force_start_immediately = true;
    params.from_scenario_editor = true;
    PushNextScreen(CreateScenario(params, &app_));
  }

  ScenarioDef def_;
  TargetManager target_manager_;
  glm::mat4 projection_;
  Theme theme_;
  float char_x_ = 0;
  ImVec2 char_size_{};

  std::vector<std::string> bundle_names_;
  std::optional<ResourceName> original_name_;
  ResourceName name_;
  Settings settings_;

  ImGui::NotificationPopup notification_popup_{"Notification"};
};

}  // namespace

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(const ScenarioEditorOptions& options,
                                                     Application* app) {
  return std::make_unique<ScenarioEditorScreen>(options, *app);
}

}  // namespace aim

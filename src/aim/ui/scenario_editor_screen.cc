#include "scenario_editor_screen.h"

#include <format>
#include <functional>
#include <optional>

#include "aim/common/field.h"
#include "aim/common/files.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/resource_name.h"
#include "aim/common/search.h"
#include "aim/common/util.h"
#include "aim/common/wall.h"
#include "aim/core/camera.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/scenario_factory.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace aim {
namespace {

struct BoundsDimensions {
  bool draw_width = true;
  bool draw_height = true;
  bool draw_depth = true;
};

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

const std::vector<std::pair<RegionLength::TypeCase, std::string>> kRegionLengthTypes{
    {RegionLength::kXPercentValue, "width"},
    {RegionLength::kYPercentValue, "height"},
    {RegionLength::kDepthPercentValue, "depth"},
};

const std::vector<std::pair<InitialDirection, std::string>> kLeftRightDirections{
    {InitialDirection::DIRECTION_POSITIVE, "Right"},
    {InitialDirection::DIRECTION_NEGATIVE, "Left"},
    {InitialDirection::DIRECTION_IN, "Towards center"},
    {InitialDirection::DIRECTION_OUT, "Away from center"},
    {InitialDirection::DIRECTION_RANDOM, "Random"},
};

const std::vector<std::pair<InitialDirection, std::string>> kUpDownDirections{
    {InitialDirection::DIRECTION_POSITIVE, "Up"},
    {InitialDirection::DIRECTION_NEGATIVE, "Down"},
    {InitialDirection::DIRECTION_IN, "Towards center"},
    {InitialDirection::DIRECTION_OUT, "Away from center"},
    {InitialDirection::DIRECTION_RANDOM, "Random"},
};

const std::vector<std::pair<InitialDirection, std::string>> kForwardBackDirections{
    {InitialDirection::DIRECTION_POSITIVE, "Forward"},
    {InitialDirection::DIRECTION_NEGATIVE, "Back"},
    {InitialDirection::DIRECTION_IN, "Towards center"},
    {InitialDirection::DIRECTION_OUT, "Away from center"},
    {InitialDirection::DIRECTION_RANDOM, "Random"},
};

const std::vector<std::pair<ShotType::TypeCase, std::string>> kShotTypes{
    {ShotType::kClickSingle, "Click"},
    {ShotType::kTrackingInvincible, "Tracking"},
    {ShotType::kTrackingKill, "Tracking kill"},
    {ShotType::kPoke, "Poke"},
    {ShotType::kClickMulti, "Multi click"},
};

const std::vector<ScenarioDef::TypeCase> kSingleTargetTrackingTypes{
    ScenarioDef::kCenteringDef,
    ScenarioDef::kWallArcDef,
    ScenarioDef::kCircleDef,
    ScenarioDef::kSineDef,
};

const std::vector<std::pair<ScenarioDef::TypeCase, std::string>> kScenarioTypes{
    {ScenarioDef::kStaticDef, "Static"},
    {ScenarioDef::kCenteringDef, "Centering"},
    {ScenarioDef::kWallStrafeDef, "Wall Strafe"},
    {ScenarioDef::kTimedDirectionDef, "Timed Direction"},
    {ScenarioDef::kBounceDef, "Bounce"},
    {ScenarioDef::kLinearDef, "Linear"},
    {ScenarioDef::kBarrelDef, "Barrel"},
    {ScenarioDef::kWallWanderDef, "Wall Wander"},
    {ScenarioDef::kWaypointDef, "Waypoint"},
    {ScenarioDef::kCircleDef, "Circle"},
    {ScenarioDef::kWallArcDef, "Wall Arc"},
    {ScenarioDef::kSineDef, "Sine"},
    {ScenarioDef::kReferenceDef, "Reference"},
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
  *r.mutable_camera_position() = ToStoredVec3(0, -200, 0);
  return r;
}

Room GetDefaultCylinderRoom() {
  Room r;
  r.mutable_cylinder_room()->set_height(130);
  r.mutable_cylinder_room()->set_radius(200);
  r.mutable_cylinder_room()->set_width(150);
  return r;
}

Room GetDefaultBarrelRoom() {
  Room r;
  r.mutable_barrel_room()->set_radius(75);
  *r.mutable_camera_position() = ToStoredVec3(0, -200, 0);
  return r;
}

TargetPlacementStrategy GetTargetPlacementStrategy(const ScenarioDef& def) {
  if (def.has_static_def()) {
    return def.static_def().target_placement_strategy();
  }
  if (def.has_waypoint_def()) {
    return def.waypoint_def().target_placement_strategy();
  }
  if (def.has_linear_def()) {
    return def.linear_def().target_placement_strategy();
  }
  if (def.has_wall_wander_def()) {
    return def.wall_wander_def().target_placement_strategy();
  }
  if (def.has_wall_strafe_def()) {
    return def.wall_strafe_def().target_placement_strategy();
  }
  if (def.has_timed_direction_def()) {
    return def.timed_direction_def().target_placement_strategy();
  }
  if (def.has_bounce_def()) {
    return def.bounce_def().target_placement_strategy();
  }
  return {};
}

class ScenarioEditorScreen : public UiScreen {
 public:
  explicit ScenarioEditorScreen(const ScenarioEditorOptions& opts, Application& app)
      : UiScreen(app),
        target_manager_(GetDefaultSimpleRoom()),
        add_to_playlist_(opts.add_to_playlist) {
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
      def_ = initial_scenario->unevaluated_def;
      name_ = initial_scenario->name;
      if (opts.force_bundle_name.size() > 0) {
        *name_.mutable_bundle_name() = opts.force_bundle_name;
      }
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
  void DrawNameEditor() {
    notification_popup_.Draw();
    ImGui::SimpleDropdown("BundlePicker", name_.mutable_bundle_name(), bundle_names_, char_x_ * 11);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 40);
    ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());
  }

  void DrawTopBar() {
    float width = char_x_ * 70;
    float middle = app_.screen_info().width / 2.0;
    ImGui::SetNextWindowPos(ImVec2(middle - width / 2.0, char_x_ / 3.0));
    ImGui::SetNextWindowSize(ImVec2(width, -1));
    if (!ImGui::Begin("TopBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
      ImGui::End();
      return;
    }

    notification_popup_.Draw();

    if (ImGui::Button(kIconPlayCircle)) {
      PlayScenario();
    }
    ImGui::HelpTooltip("Try playing the edited version of the scenario.");

    ImGui::SameLine();
    ImGui::SimpleDropdown("BundlePicker", name_.mutable_bundle_name(), bundle_names_, char_x_ * 11);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 40);
    ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());

    ImGui::SameLine();
    bool is_new_scenario = !original_name_.has_value();
    std::string save_text =
        is_new_scenario ? std::format("{} Create", kIconAdd) : std::format("{} Update", kIconSave);
    if (ImGui::Button(save_text, ImVec2(char_x_ * 8, 0))) {
      if (SaveScenario()) {
        app_.scenario_manager().LoadScenariosFromDisk();
        app_.playlist_manager().LoadPlaylistsFromDisk();
        PopSelf();
      }
    }
    ImGui::SameLine();
    ImGui::SetButtonCursorAtRight("Cancel");
    if (ImGui::Button("Cancel")) {
      PopSelf();
    }

    ImGui::End();
  }

  void DrawDescriptionEditor() {
    if (ImGui::Button(std::format("{} Back to editor", kIconArrowBack))) {
      editing_description_ = false;
    }
    ImGui::InputTextMultiline("##DescriptionInput",
                              def_.mutable_description(),
                              ImGui::GetContentRegionAvail(),
                              ImGuiInputTextFlags_AllowTabInput);
  }

  void DrawMainEditor() {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("MainEditorColumns", 3, flags)) {
      // TODO: make each column default size to 1/3
      ImGui::TableNextColumn();
      DrawDetailsEditor();

      ImGui::TableNextColumn();
      ImGui::BeginChild("SecondColumnContainer", ImVec2(0, 0));
      DrawScenarioTypeEditor();
      ImGui::EndChild();

      ImGui::TableNextColumn();
      DrawTargetEditor();

      ImGui::EndTable();
    }
  }

  void DrawScreen() override {
    ImGui::IdGuard cid("ScenarioEditor");
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_size_ = char_size;
    char_x_ = char_size_.x;

    DrawTopBar();

    if (def_.room().type_case() == Room::TYPE_NOT_SET) {
      *def_.mutable_room() = GetDefaultSimpleRoom();
    }

    if (editing_room_) {
      DrawRoomEditor();
      return;
    }

    float padding = char_x_ * 0.3;
    float editor_start_y = ImGui::GetCursorPosY() + ImGui::GetTextLineHeight() * 1;
    float editor_end_y = app_.screen_info().height - padding;

    if (editing_description_) {
      if (BeginMainWindow("DescriptionEditor", 0.6)) {
        DrawDescriptionEditor();
      }
      ImGui::End();
      return;
    }

    if (def_.has_reference_def()) {
      if (BeginMainWindow("ReferenceEditor", 0.6)) {
        DrawReferenceEditor();
      }
      ImGui::End();
      return;
    }

    if (BeginMainWindow("MainEditor", 0.9)) {
      DrawMainEditor();
    }
    ImGui::End();
  }

  void DrawDetailsEditor() {
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

    Line();

    ImGui::Text("Overrides");
    ImGui::Indent();
    DrawOverridesEditor();
    if (ImGui::Button("Bake")) {
      def_ = ApplyScenarioOverrides(def_);
    }
    ImGui::SameLine();
    ImGui::HelpMarker("Apply and remove the overrides.");
    ImGui::Unindent();

    Line();

    if (ImGui::Button("Edit room")) {
      editing_room_ = true;
    }

    if (ImGui::Button("Edit description")) {
      editing_description_ = true;
    }

    Line();

    if (original_name_.has_value()) {
      if (ImGui::Button("Make changes in new copy")) {
        original_name_ = {};
        *name_.mutable_relative_name() = name_.relative_name() + " Copy";
      }
      ImGui::HelpTooltip(
          "Save the current changes in a new copy of the scenario leaving the original unchanged.");
    }

    if (ImGui::Button("View Json")) {
      SetErrorMessage(MessageToJson(def_, 6));
    }
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
    app_.history_manager().UpdateRecentView(ObjectType::SCENARIO, name_.full_name());
    if (add_to_playlist_.size() > 0) {
      app_.playlist_manager().AddScenarioToPlaylist(add_to_playlist_, name_.full_name());
    }
    return true;
  }

  void SetErrorMessage(const std::string& msg) {
    notification_popup_.NotifyOpen(msg);
  }

  void DrawScenarioTypeEditor() {
    ImGui::IdGuard cid("ScenarioTypeEditor");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scenario type");
    ImGui::SameLine();

    if (def_.type_case() == ScenarioDef::TYPE_NOT_SET) {
      def_.mutable_static_def();
    }

    auto scenario_type = def_.type_case();
    bool is_new_type = ImGui::SimpleTypeDropdown(
        "ScenarioTypeDropdown", &scenario_type, kScenarioTypes, char_x_ * 15);
    InitializeScenarioType(scenario_type);

    if (VectorContains(kSingleTargetTrackingTypes, scenario_type)) {
      def_.mutable_shot_type()->set_tracking_invincible(true);
      if (is_new_type) {
        def_.clear_target_def();
      }
    } else {
      DrawShotTypeEditor();
    }

    Line();

    if (scenario_type == ScenarioDef::kStaticDef) {
      DrawStaticEditor();
    }
    if (scenario_type == ScenarioDef::kWaypointDef) {
      DrawWaypointEditor();
    }
    if (scenario_type == ScenarioDef::kCenteringDef) {
      DrawCenteringEditor();
    }
    if (scenario_type == ScenarioDef::kWallStrafeDef) {
      DrawWallStrafeEditor();
    }
    if (scenario_type == ScenarioDef::kTimedDirectionDef) {
      DrawTimedDirectionEditor();
    }
    if (scenario_type == ScenarioDef::kBounceDef) {
      DrawBounceEditor();
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
    if (scenario_type == ScenarioDef::kWallWanderDef) {
      DrawWallWanderEditor();
    }
    if (scenario_type == ScenarioDef::kCircleDef) {
      DrawCircleEditor();
    }
    if (scenario_type == ScenarioDef::kSineDef) {
      DrawSineEditor();
    }
  }

  void InitializeScenarioType(ScenarioDef::TypeCase scenario_type) {
    auto target_placement = GetTargetPlacementStrategy(def_);
    if (scenario_type == ScenarioDef::kStaticDef) {
      *def_.mutable_static_def()->mutable_target_placement_strategy() = target_placement;
    }
    if (scenario_type == ScenarioDef::kWaypointDef) {
      *def_.mutable_waypoint_def()->mutable_target_placement_strategy() = target_placement;
    }
    if (scenario_type == ScenarioDef::kCenteringDef) {
      def_.mutable_centering_def();
    }
    if (scenario_type == ScenarioDef::kWallStrafeDef) {
      auto* wall_strafe = def_.mutable_wall_strafe_def();
      if (target_placement.regions_size() > 0) {
        *wall_strafe->mutable_target_placement_strategy() = target_placement;
      }
    }
    if (scenario_type == ScenarioDef::kTimedDirectionDef) {
      auto* timed_direction = def_.mutable_timed_direction_def();
      if (target_placement.regions_size() > 0) {
        *timed_direction->mutable_target_placement_strategy() = target_placement;
      }
    }
    if (scenario_type == ScenarioDef::kBounceDef) {
      auto* d = def_.mutable_bounce_def();
      if (target_placement.regions_size() > 0) {
        *d->mutable_target_placement_strategy() = target_placement;
      }
    }
    if (scenario_type == ScenarioDef::kLinearDef) {
      *def_.mutable_linear_def()->mutable_target_placement_strategy() = target_placement;
    }
    if (scenario_type == ScenarioDef::kBarrelDef) {
      def_.mutable_barrel_def();
    }
    if (scenario_type == ScenarioDef::kWallArcDef) {
      def_.mutable_wall_arc_def();
    }
    if (scenario_type == ScenarioDef::kReferenceDef) {
      def_.mutable_reference_def();
    }
  }

  bool BeginMainWindow(const std::string& name, float width_multiple) {
    float padding = char_x_ * 0.3;
    float start_y = ImGui::GetCursorPosY() + ImGui::GetTextLineHeight() * 1;
    float end_y = app_.screen_info().height - padding;
    float width = app_.screen_info().width * width_multiple;
    float height = end_y - start_y;

    ImGui::SetNextWindowPos(ImVec2((app_.screen_info().width - width) / 2.0, start_y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    return ImGui::Begin(
        name.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
  }

  void DrawReferenceEditor() {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scenario type");
    ImGui::SameLine();

    auto scenario_type = def_.type_case();
    ImGui::SimpleTypeDropdown("ScenarioTypeDropdown", &scenario_type, kScenarioTypes, char_x_ * 15);
    InitializeScenarioType(scenario_type);

    if (scenario_type != ScenarioDef::kReferenceDef) {
      return;
    }

    Line();

    // Make sure only the appropriate fields are set on the def.
    ScenarioDef old_def = def_;

    def_ = {};
    def_.set_description(old_def.description());
    *def_.mutable_overrides() = old_def.overrides();
    *def_.mutable_reference_def() = old_def.reference_def();

    ReferenceScenarioDef& r = *def_.mutable_reference_def();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scenario");
    ImGui::SameLine();
    ImGui::HelpMarker("The name of the scenario to reference");
    ImGui::SameLine();
    ImGui::InputText("##ScenarioReference", r.mutable_scenario_id());

    if (r.scenario_id().size() > 0) {
      auto matching_scenario = app_.scenario_manager().GetScenario(r.scenario_id());
      if (!matching_scenario) {
        // Show search results for scenarios.
        int num_matches = 0;
        auto search_words = GetSearchWords(r.scenario_id());
        ImGui::Indent();
        for (const auto& scenario : app_.scenario_manager().scenarios()) {
          if (StringMatchesSearch(scenario.id(), search_words)) {
            num_matches++;
            if (ImGui::Button(scenario.id())) {
              r.set_scenario_id(scenario.id());
            }
          }
        }
        if (num_matches == 0) {
          ImGui::Text("No matching scenarios found");
        }
        ImGui::Unindent();
      }
    }

    Line();

    ImGui::Text("Overrides");
    ImGui::Indent();
    DrawOverridesEditor();
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Bake")) {
      auto parent = app_.scenario_manager().GetEvaluatedScenario(r.scenario_id());
      if (parent) {
        auto overrides = def_.overrides();
        def_ = parent->def;
        *def_.mutable_overrides() = overrides;
        def_ = ApplyScenarioOverrides(def_);
      } else {
        SetErrorMessage(std::format("Referenced scenario \"{}\" is invalid.", r.scenario_id()));
      }
    }
    ImGui::SameLine();
    ImGui::HelpMarker("Expand and remove the reference. Will now be an equivalent normal scenario");
  }

  void DrawOverridesEditor() {
    ImGui::InputFloat(
        ImGui::InputFloatParams("TargetRadiusMult")
            .set_label("Target radius multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(ScenarioOverrides, def_.mutable_overrides(), target_radius_multiplier));

    ImGui::InputFloat(
        ImGui::InputFloatParams("SpeedMult")
            .set_label("Speed multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(ScenarioOverrides, def_.mutable_overrides(), speed_multiplier));

    ImGui::InputFloat(
        ImGui::InputFloatParams("AccelMult")
            .set_label("Acceleration multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(ScenarioOverrides, def_.mutable_overrides(), acceleration_multiplier));

    ImGui::InputFloat(
        ImGui::InputFloatParams("TimeScale")
            .set_label("Time scale multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(ScenarioOverrides, def_.mutable_overrides(), time_scale_multiplier));
    ImGui::InputFloat(
        ImGui::InputFloatParams("Distance")
            .set_label("Distance multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(ScenarioOverrides, def_.mutable_overrides(), distance_multiplier));

    if (def_.has_reference_def()) {
      // Allow overriding full set of fields for references.
      ImGui::InputInt(ImGui::InputIntParams("NumberOfTargets")
                          .set_label("Number of targets")
                          .set_step(1, 2)
                          .set_min(1)
                          .set_default(1)
                          .set_is_optional()
                          .set_width(char_x_ * 10),
                      PROTO_INT_FIELD(ScenarioOverrides, def_.mutable_overrides(), num_targets));
    }
  }

  void DrawWallArcEditor() {
    ImGui::IdGuard cid("WallArcEditor");
    WallArcScenarioDef& d = *def_.mutable_wall_arc_def();

    ImGui::InputFloat(ImGui::InputFloatParams("ControlHeight")
                          .set_label("Control height")
                          .set_step(0.1, 2)
                          .set_precision(1)
                          .set_default(2)
                          .set_min(0.1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(WallArcScenarioDef, &d, control_height));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "The arc is defined by a quadratic bezier curve where start is (0, 0) and end is (2, 0). "
        "The specified height is for the control point (1, height).");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Width");
    ImGui::SameLine();
    DrawRegionLengthEditor("Width", DefaultDim::DIM_X, d.mutable_width());
    ImGui::SameLine();
    ImGui::HelpMarker("The arc will be stretched over the specified width");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Height");
    ImGui::SameLine();
    DrawRegionLengthEditor("Height", DefaultDim::DIM_Y, d.mutable_height());

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Height range");
    ImGui::SameLine();
    bool use_range = d.has_height_jitter();
    ImGui::Checkbox("##UseRange", &use_range);
    if (use_range) {
      ImGui::SameLine();
      DrawRegionLengthEditor("Height range", DefaultDim::DIM_Y, d.mutable_height_jitter());
    } else {
      d.clear_height_jitter();
    }

    ImGui::InputBool(ImGui::InputBoolParams("Reflect").set_label("Reflect"),
                     PROTO_BOOL_FIELD(WallArcScenarioDef, &d, reflect));
    ImGui::SameLine();
    ImGui::HelpMarker("Turn the arc upside down.");

    ImGui::InputBool(ImGui::InputBoolParams("StartOnGround").set_label("Start on ground"),
                     PROTO_BOOL_FIELD(WallArcScenarioDef, &d, start_on_ground));
  }

  void DrawSineEditor() {
    ImGui::IdGuard cid("SineEditor");
    SineScenarioDef& d = *def_.mutable_sine_def();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Height");
    ImGui::SameLine();
    DrawRegionLengthEditor("Height", DefaultDim::DIM_Y, d.mutable_height());

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Width");
    ImGui::SameLine();
    DrawRegionLengthEditor("Width", DefaultDim::DIM_X, d.mutable_width());

    ImGui::InputBool(ImGui::InputBoolParams("GoingRight").set_label("Going left"),
                     PROTO_BOOL_FIELD(SineScenarioDef, &d, going_left));
  }

  void DrawCircleEditor() {
    ImGui::IdGuard cid("CircleEditor");
    CircleScenarioDef& d = *def_.mutable_circle_def();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Radius");
    ImGui::SameLine();
    DrawRegionLengthEditor("Radius", DefaultDim::DIM_X, d.mutable_radius());

    ImGui::InputFloat(ImGui::InputFloatParams("StartDegrees")
                          .set_label("Start degrees")
                          .set_step(5, 30)
                          .set_precision(0)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(CircleScenarioDef, &d, start_degrees));
    ImGui::SameLine();
    ImGui::HelpMarker("0 degrees starts at 3 o'clock and rotates counter clockwise.");

    ImGui::InputBool(ImGui::InputBoolParams("Clockwise").set_label("Clockwise"),
                     PROTO_BOOL_FIELD(CircleScenarioDef, &d, rotate_clockwise));

    Line();

    ImGui::InputFloat(ImGui::InputFloatParams("StretchY")
                          .set_label("Stretch Y")
                          .set_step(0.05, 0.1)
                          .set_precision(2)
                          .set_default(0.8)
                          .set_min(0.1)
                          .set_is_optional()
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(CircleScenarioDef, &d, stretch_y));
    ImGui::InputFloat(ImGui::InputFloatParams("StretchX")
                          .set_label("Stretch X")
                          .set_step(0.05, 0.1)
                          .set_precision(2)
                          .set_default(0.8)
                          .set_min(0.1)
                          .set_is_optional()
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(CircleScenarioDef, &d, stretch_x));

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Depth");
    ImGui::SameLine();
    DrawRegionLengthEditor("Depth", DefaultDim::DIM_DEPTH, d.mutable_depth());
    ImGui::SameLine();
    ImGui::HelpMarker("Distance away from the wall");
  }

  void DrawWallWanderProfile(WallWanderProfile* p) {
    ImGui::InputJitteredFloat(ImGui::InputFloatParams("TimeBetweenTurns")
                                  .set_label("Time between turns")
                                  .set_step(0.1, 2)
                                  .set_min(0.1)
                                  .set_precision(1)
                                  .set_default(2)
                                  .set_width(char_x_ * 10),
                              PROTO_JITTERED_FIELD(WallWanderProfile, p, turn_time));
    ImGui::SameLine();
    ImGui::HelpMarker("The amount of time to turn in a single direction before switching.");

    ImGui::InputJitteredFloat(ImGui::InputFloatParams("TurnRate")
                                  .set_label("Turn rate")
                                  .set_step(10, 30)
                                  .set_default(300)
                                  .set_min(0)
                                  .set_width(char_x_ * 10),
                              PROTO_JITTERED_FIELD(WallWanderProfile, p, turn_rate));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "The number of degrees to turn per second. The turn rate will accelerate smoothly between "
        "turns base on turn time.");
  }

  void DrawWallWanderEditor() {
    ImGui::IdGuard cid("WallWanderEditor");
    WallWanderScenarioDef& d = *def_.mutable_wall_wander_def();

    if (d.profiles_size() == 0) {
      d.add_profiles();
    }
    ImGui::Text("Wander profiles");
    ImGui::Indent();
    DrawProfileList("WanderProfileList",
                    "Profile",
                    d.mutable_profile_order(),
                    d.mutable_profiles(),
                    std::bind_front(&ScenarioEditorScreen::DrawWallWanderProfile, this));
    ImGui::Unindent();

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial target location");
    bool has_location = d.has_target_placement_strategy();
    ImGui::SameLine();
    ImGui::Checkbox("##UseInitial", &has_location);
    if (has_location) {
      ImGui::Indent();
      DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
      ImGui::Unindent();
    } else {
      d.clear_target_placement_strategy();
    }
  }

  void DrawLinearEditor() {
    ImGui::IdGuard cid("LinearEditor");
    LinearScenarioDef& d = *def_.mutable_linear_def();

    ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Angle")
                                  .set_step(1, 3)
                                  .set_min(0)
                                  .set_max(60)
                                  .set_precision(0)
                                  .set_default(0)
                                  .set_width(char_x_ * 10),
                              PROTO_JITTERED_FIELD(LinearScenarioDef, &d, angle));

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial left/right direction");
    ImGui::SameLine();
    InitialDirection left_right_direction_type = d.has_left_right_initial_direction()
                                                     ? d.left_right_initial_direction()
                                                     : InitialDirection::DIRECTION_IN;
    ImGui::SimpleTypeDropdown("LeftRightDirectionTypeDropdown",
                              &left_right_direction_type,
                              kLeftRightDirections,
                              char_x_ * 20);
    d.set_left_right_initial_direction(left_right_direction_type);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial up/down direction");
    ImGui::SameLine();
    InitialDirection up_down_direction_type = d.up_down_initial_direction();
    ImGui::SimpleTypeDropdown(
        "UpDownDirectionTypeDropdown", &up_down_direction_type, kUpDownDirections, char_x_ * 20);
    d.set_up_down_initial_direction(up_down_direction_type);

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial target location");
    ImGui::Indent();
    DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
    ImGui::Unindent();
  }

  void DrawBarrelEditor() {
    ImGui::IdGuard cid("BarrelEditor");
    BarrelScenarioDef& d = *def_.mutable_barrel_def();

    if (!def_.room().has_barrel_room()) {
      ImGui::Text("Must use barrel room");
      return;
    }

    ImGui::InputFloat(ImGui::InputFloatParams("DirectionRadiusPercent")
                          .set_label("Redirect to percent of center")
                          .set_step(1, 5)
                          .set_min(1)
                          .set_precision(0)
                          .set_default(40)
                          .set_width(char_x_ * 10),
                      PROTO_PERCENT_FIELD(BarrelScenarioDef, &d, direction_radius_percent));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "When the target collides with the wall it will be redirected in the direction of a random "
        "point within the specified portion of the center. The smaller the radius the more it will "
        "be redirected towards the center of the circle.");

    if (!d.has_target_placement_strategy()) {
      d.mutable_target_placement_strategy()->mutable_min_distance()->set_value(15);
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

  void DrawTimedDirectionProfile(TimedDirectionProfile* p) {
    ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Time")
                                  .set_step(0.1, 0.5)
                                  .set_min(0.1)
                                  .set_precision(2)
                                  .set_default(1)
                                  .set_width(char_x_ * 10),
                              PROTO_JITTERED_FIELD(TimedDirectionProfile, p, time));
    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Speed multiplier")
                          .set_is_optional()
                          .set_step(0.05, 0.2)
                          .set_min(0)
                          .set_precision(2)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(TimedDirectionProfile, p, speed_multiplier));
    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Acceleration multiplier")
                          .set_is_optional()
                          .set_step(0.05, 0.2)
                          .set_min(0)
                          .set_precision(2)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(TimedDirectionProfile, p, acceleration_multiplier));
  }

  void DrawTimedDirectionEditor() {
    ImGui::IdGuard cid("TimedDirectonEditor");
    TimedDirectionScenarioDef& d = *def_.mutable_timed_direction_def();
    DrawBoundsEditor("##Bounds", d.mutable_bounds());

    Line();

    ImGui::InputFloat(ImGui::InputFloatParams("TimeScaleMultiplier")
                          .set_label("Time scale multiplier")
                          .set_is_optional()
                          .set_step(0.05, 0.1)
                          .set_min(0.01)
                          .set_precision(2)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(TimedDirectionScenarioDef, &d, time_scale_multiplier));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Scale all the times in the profiles by the given multiplier. To reduce the times by half "
        "use 0.5");

    ImGui::InputFloat(ImGui::InputFloatParams("Acceleration")
                          .set_label("Acceleration")
                          .set_is_optional()
                          .set_step(5, 50)
                          .set_min(1)
                          .set_precision(0)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(TimedDirectionScenarioDef, &d, acceleration));

    Line();

    ImGui::Text("Left/right profiles");
    ImGui::Indent();
    DrawProfileList("LeftRightProfileList",
                    "Profile",
                    d.mutable_left_right_profile_order(),
                    d.mutable_left_right_profiles(),
                    std::bind_front(&ScenarioEditorScreen::DrawTimedDirectionProfile, this));
    ImGui::Unindent();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial left/right direction");
    ImGui::SameLine();
    InitialDirection left_right_direction = d.left_right_initial_direction();
    ImGui::SimpleTypeDropdown("LeftRightDirectionTypeDropdown",
                              &left_right_direction,
                              kLeftRightDirections,
                              char_x_ * 18);
    d.set_left_right_initial_direction(left_right_direction);

    Line();

    ImGui::Text("Up/down profiles");
    ImGui::Indent();
    DrawProfileList("UpDownProfileList",
                    "Profile",
                    d.mutable_up_down_profile_order(),
                    d.mutable_up_down_profiles(),
                    std::bind_front(&ScenarioEditorScreen::DrawTimedDirectionProfile, this));
    ImGui::Unindent();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial up/down direction");
    ImGui::SameLine();
    InitialDirection up_down_direction = d.up_down_initial_direction();
    ImGui::SimpleTypeDropdown(
        "UpDownDirectionTypeDropdown", &up_down_direction, kUpDownDirections, char_x_ * 18);
    d.set_up_down_initial_direction(up_down_direction);

    if (d.bounds().has_depth()) {
      Line();
      ImGui::Text("Forward/back profiles");
      ImGui::Indent();
      DrawProfileList("ForwardBackProfileList",
                      "Profile",
                      d.mutable_forward_back_profile_order(),
                      d.mutable_forward_back_profiles(),
                      std::bind_front(&ScenarioEditorScreen::DrawTimedDirectionProfile, this));
      ImGui::Unindent();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Initial forward/back direction");
      ImGui::SameLine();
      InitialDirection forward_back_direction = d.forward_back_initial_direction();
      ImGui::SimpleTypeDropdown("ForwardBackDirectionTypeDropdown",
                                &forward_back_direction,
                                kForwardBackDirections,
                                char_x_ * 18);
      d.set_forward_back_initial_direction(forward_back_direction);
    } else {
      d.clear_forward_back_profiles();
      d.clear_forward_back_profile_order();
      d.clear_forward_back_initial_direction();
    }

    Line();

    bool use_target_placement = d.has_target_placement_strategy();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Set initial target location");
    ImGui::SameLine();
    ImGui::Checkbox("##UseTargetPlacement", &use_target_placement);
    if (use_target_placement) {
      ImGui::Indent();
      DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
      ImGui::Unindent();
    } else {
      d.clear_target_placement_strategy();
    }
  }

  void DrawBounceProfile(BounceProfile* p) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Bounce height");
    ImGui::SameLine();
    DrawRegionLengthEditor("BounceHeight", DefaultDim::DIM_Y, p->mutable_height());

    ImGui::AlignTextToFramePadding();
    ImGui::Indent();
    ImGui::Text("+/-");
    ImGui::SameLine();
    DrawRegionLengthEditor(
        "BounceHeightJitter", DefaultDim::DIM_Y, p->mutable_height_jitter(), false, true);
    ImGui::Unindent();

    ImGui::InputJitteredFloat(ImGui::InputFloatParams("Delay")
                                  .set_label("Bounce delay")
                                  .set_step(0.05, 0.2)
                                  .set_zero_is_unset()
                                  .set_min(0)
                                  .set_precision(2)
                                  .set_default(0)
                                  .set_width(char_x_ * 10),
                              PROTO_JITTERED_FIELD(BounceProfile, p, delay_seconds));
    ImGui::InputBool(ImGui::InputBoolParams("OnlyDelayOnFloor")
                         .set_label("Only delay on floor")
                         .set_false_is_unset(),
                     PROTO_BOOL_FIELD(BounceProfile, p, only_delay_on_floor));

    ImGui::InputFloat(ImGui::InputFloatParams("SpeedMultiplier")
                          .set_label("Speed multiplier")
                          .set_is_optional()
                          .set_step(0.05, 0.2)
                          .set_min(0)
                          .set_precision(2)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(BounceProfile, p, speed_multiplier));
    ImGui::InputFloat(ImGui::InputFloatParams("AccelerationMultiplier")
                          .set_label("Acceleration multiplier")
                          .set_is_optional()
                          .set_step(0.05, 0.2)
                          .set_min(0)
                          .set_precision(2)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(BounceProfile, p, acceleration_multiplier));
  }

  void DrawBounceEditor() {
    ImGui::IdGuard cid("BounceEditor");
    BounceScenarioDef& d = *def_.mutable_bounce_def();
    BoundsDimensions dimensions;
    dimensions.draw_height = false;
    DrawBoundsEditor("##Bounds", d.mutable_bounds(), dimensions);

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Floor height");
    ImGui::SameLine();
    bool use_floor_height = d.has_floor_height();
    ImGui::SameLine();
    ImGui::Checkbox("##FloorHeight", &use_floor_height);
    if (use_floor_height) {
      ImGui::SameLine();
      DrawRegionLengthEditor("FloorHeight", DefaultDim::DIM_Y, d.mutable_floor_height());
    } else {
      d.clear_floor_height();
    }

    if (d.bounce_profiles_size() == 0) {
      d.add_bounce_profiles();
    }
    ImGui::Text("Bounce profiles");
    ImGui::Indent();
    DrawProfileList("BounceProfileList",
                    "Profile",
                    d.mutable_bounce_profile_order(),
                    d.mutable_bounce_profiles(),
                    std::bind_front(&ScenarioEditorScreen::DrawBounceProfile, this));
    ImGui::Unindent();

    Line();

    ImGui::InputFloat(ImGui::InputFloatParams("TimeScaleMultiplier")
                          .set_label("Time scale multiplier")
                          .set_is_optional()
                          .set_step(0.05, 0.1)
                          .set_min(0.01)
                          .set_precision(2)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(BounceScenarioDef, &d, time_scale_multiplier));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Scale all the times in the profiles by the given multiplier. To reduce the times by half "
        "use 0.5");

    ImGui::InputFloat(ImGui::InputFloatParams("Acceleration")
                          .set_label("Acceleration")
                          .set_is_optional()
                          .set_step(5, 50)
                          .set_min(1)
                          .set_precision(0)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(BounceScenarioDef, &d, acceleration));

    Line();

    ImGui::Text("Left/right profiles");
    ImGui::Indent();
    DrawProfileList("LeftRightProfileList",
                    "Profile",
                    d.mutable_left_right_profile_order(),
                    d.mutable_left_right_profiles(),
                    std::bind_front(&ScenarioEditorScreen::DrawTimedDirectionProfile, this));
    ImGui::Unindent();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial left/right direction");
    ImGui::SameLine();
    InitialDirection left_right_direction = d.left_right_initial_direction();
    ImGui::SimpleTypeDropdown("LeftRightDirectionTypeDropdown",
                              &left_right_direction,
                              kLeftRightDirections,
                              char_x_ * 18);
    d.set_left_right_initial_direction(left_right_direction);

    if (d.bounds().has_depth()) {
      Line();
      ImGui::Text("Forward/back profiles");
      ImGui::Indent();
      DrawProfileList("ForwardBackProfileList",
                      "Profile",
                      d.mutable_forward_back_profile_order(),
                      d.mutable_forward_back_profiles(),
                      std::bind_front(&ScenarioEditorScreen::DrawTimedDirectionProfile, this));
      ImGui::Unindent();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Initial forward/back direction");
      ImGui::SameLine();
      InitialDirection forward_back_direction = d.forward_back_initial_direction();
      ImGui::SimpleTypeDropdown("ForwardBackDirectionTypeDropdown",
                                &forward_back_direction,
                                kForwardBackDirections,
                                char_x_ * 18);
      d.set_forward_back_initial_direction(forward_back_direction);
    } else {
      d.clear_forward_back_profiles();
      d.clear_forward_back_profile_order();
      d.clear_forward_back_initial_direction();
    }

    Line();

    bool use_target_placement = d.has_target_placement_strategy();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Set initial target location");
    ImGui::SameLine();
    ImGui::Checkbox("##UseTargetPlacement", &use_target_placement);
    if (use_target_placement) {
      ImGui::Indent();
      DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
      ImGui::Unindent();
    } else {
      d.clear_target_placement_strategy();
    }
  }

  void DrawWallStrafeEditor() {
    ImGui::IdGuard cid("WallStrafeEditor");
    WallStrafeScenarioDef& w = *def_.mutable_wall_strafe_def();
    DrawBoundsEditor("##Bounds", w.mutable_bounds());

    if (w.profiles_size() == 0) {
      w.add_profiles();
    }

    Line();

    ImGui::InputFloat(ImGui::InputFloatParams("Acceleration")
                          .set_label("Acceleration")
                          .set_is_optional()
                          .set_step(5, 50)
                          .set_min(1)
                          .set_precision(0)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(WallStrafeScenarioDef, &w, acceleration));
    ImGui::SameLine();
    ImGui::HelpMarker("The target will accelerate in and out of changes of direction");

    ImGui::InputFloat(ImGui::InputFloatParams("DistanceMult")
                          .set_label("Distance multiplier")
                          .set_is_optional()
                          .set_step(0.01, 0.1)
                          .set_min(0.01)
                          .set_precision(2)
                          .set_default(1)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(WallStrafeScenarioDef, &w, distance_multiplier));
    ImGui::SameLine();
    ImGui::HelpMarker("Multiply all strafe distances by the provided value");

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

    bool use_target_placement = w.has_target_placement_strategy();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Set initial target location");
    ImGui::SameLine();
    ImGui::Checkbox("##UseTargetPlacement", &use_target_placement);
    if (use_target_placement) {
      ImGui::Indent();
      DrawTargetPlacementStrategyEditor("Placement", w.mutable_target_placement_strategy());
      ImGui::Unindent();
    } else {
      w.clear_target_placement_strategy();
    }
  }

  void DrawWallStrafeProfile(WallStrafeProfile* p) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Distance");
    ImGui::SameLine();
    DrawJitteredRegionLengthEditor(
        "Distance", DefaultDim::DIM_X, p->mutable_distance(), p->mutable_distance_jitter());

    ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Angle")
                                  .set_step(1, 3)
                                  .set_min(0)
                                  .set_max(60)
                                  .set_precision(0)
                                  .set_default(0)
                                  .set_width(char_x_ * 10),
                              PROTO_JITTERED_FIELD(WallStrafeProfile, p, angle));

    if (p->angle() > 0 || p->angle_jitter() > 0) {
      ImGui::InputFloat(ImGui::InputFloatParams("DirectionChangePercent")
                            .set_label("Direction change chance")
                            .set_step(1, 5)
                            .set_range(0, 100)
                            .set_precision(0)
                            .set_default(50)
                            .set_width(char_x_ * 12),
                        PROTO_PERCENT_FIELD(WallStrafeProfile, p, direction_change_percent));
    } else {
      p->clear_direction_change_percent();
    }

    ImGui::InputFloat(ImGui::InputFloatParams("SpeedMultiplier")
                          .set_label("Speed multiplier")
                          .set_is_optional()
                          .set_step(.05, .2)
                          .set_min(0)
                          .set_precision(2)
                          .set_default(1)
                          .set_width(char_x_ * 12),
                      PROTO_FLOAT_FIELD(WallStrafeProfile, p, speed_multiplier));
    ImGui::InputFloat(ImGui::InputFloatParams("AccelMultiplier")
                          .set_label("Acceleration multiplier")
                          .set_is_optional()
                          .set_step(.05, .2)
                          .set_min(0)
                          .set_precision(2)
                          .set_default(1)
                          .set_width(char_x_ * 12),
                      PROTO_FLOAT_FIELD(WallStrafeProfile, p, acceleration_multiplier));

    bool is_pause = p->pause_at_end_chance() > 0;
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Pause at end");
    ImGui::SameLine();
    ImGui::Checkbox("##PauseCheck", &is_pause);
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Specify a probability that the target will stop for a certain duration at the end of "
        "this strafe before changing direction.");
    if (is_pause) {
      ImGui::Indent();
      ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Percent chance")
                            .set_step(1, 10)
                            .set_min(0)
                            .set_max(100)
                            .set_default(50)
                            .set_precision(0)
                            .set_width(char_x_ * 10),
                        PROTO_PERCENT_FIELD(WallStrafeProfile, p, pause_at_end_chance));

      ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Pause seconds")
                                    .set_step(0.05, .25)
                                    .set_min(0.01)
                                    .set_precision(2)
                                    .set_default(0.3)
                                    .set_width(char_x_ * 10),
                                PROTO_JITTERED_FIELD(WallStrafeProfile, p, pause_seconds));
    } else {
      p->clear_pause_seconds();
      p->clear_pause_seconds_jitter();
      p->clear_pause_at_end_chance();
    }
  }

  void DrawCenteringEditor() {
    ImGui::IdGuard cid("CenteringEditor");
    CenteringScenarioDef& c = *def_.mutable_centering_def();

    const char* kPoints = "Points";
    const char* kAngle = "Angle";

    std::string type = kPoints;
    if (c.has_angle()) {
      type = kAngle;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Type");
    ImGui::SameLine();
    ImGui::SimpleDropdown("##TypeDrop", &type, {kPoints, kAngle}, char_x_ * 13);

    bool use_angle = type == kAngle;

    if (use_angle) {
      c.clear_wall_points();
    } else {
      c.clear_angle();
      c.clear_angle_length();
    }

    if (use_angle) {
      ImGui::Indent();
      ImGui::InputJitteredFloat(ImGui::InputFloatParams("Angle")
                                    .set_label("Angle degrees")
                                    .set_step(1, 5)
                                    .set_width(char_x_ * 12),
                                PROTO_JITTERED_FIELD(CenteringScenarioDef, &c, angle));
      ImGui::SameLine();
      ImGui::HelpMarker(
          "Specify just the angle of movement and how far to travel. Typically used with Barrel "
          "rooms.");
      DrawRegionLengthEditor("Length", DefaultDim::DIM_X, c.mutable_angle_length());
      ImGui::Unindent();
    } else {
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

      int remove_at_i = -1;
      for (int i = 2; i < c.wall_points_size(); ++i) {
        ImGui::IdGuard lid("ExtraPoint", i);
        ImGui::AlignTextToFramePadding();
        ImGui::TextFmt("Point {}", i + 1);
        ImGui::SameLine();
        if (ImGui::Button(kIconCancel)) {
          remove_at_i = i;
        }
        ImGui::Indent();
        DrawRegionVec2Editor("##PointEditor", c.mutable_wall_points(i));
        ImGui::Unindent();
      }

      ImGui::Spacing();
      if (ImGui::Button("Add point")) {
        c.add_wall_points();
      }

      if (remove_at_i > 0) {
        c.mutable_wall_points()->erase(c.mutable_wall_points()->begin() + remove_at_i);
      }
    }
  }

  void DrawStaticEditor() {
    ImGui::IdGuard cid("StaticEditor");
    DrawTargetPlacementStrategyEditor(
        "Placement", def_.mutable_static_def()->mutable_target_placement_strategy());
  }

  void DrawWaypointEditor() {
    ImGui::IdGuard cid("WaypointEditor");
    DrawTargetPlacementStrategyEditor(
        "Placement", def_.mutable_waypoint_def()->mutable_target_placement_strategy());
  }

  void DrawTargetPlacementStrategyEditor(const std::string& id,
                                         TargetPlacementStrategy* s,
                                         bool support_depth = true) {
    auto strat = s->DebugString();
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
                    std::bind_front(&ScenarioEditorScreen::DrawTargetRegion, this, support_depth));
    ImGui::Unindent();

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Min distance");
    ImGui::SameLine();

    DrawOptionalRegionLengthEditor(
        "MinDistanceInput",
        DefaultDim::DIM_X,
        PROTO_PTR_FIELD(RegionLength, TargetPlacementStrategy, s, min_distance),
        false,
        true);
    ImGui::SameLine();
    ImGui::HelpMarker("Minimum distance between targets.");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Fixed distance");
    ImGui::SameLine();
    DrawOptionalRegionLengthEditor(
        "FixedDistanceInput",
        DefaultDim::DIM_X,
        PROTO_PTR_FIELD(RegionLength, TargetPlacementStrategy, s, fixed_distance_from_last_target));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "New target will be placed at a fixed distance from the last target that was added.");
  }

  void DrawTargetRegion(bool support_depth, TargetRegion* region) {
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
      DrawRegionLengthEditor("Width", DefaultDim::DIM_X, t->mutable_x_length());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Height");
      ImGui::SameLine();
      DrawRegionLengthEditor("Height", DefaultDim::DIM_Y, t->mutable_y_length());

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
        DrawRegionLengthEditor("Width", DefaultDim::DIM_X, t->mutable_inner_x_length());

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Height");
        ImGui::SameLine();
        DrawRegionLengthEditor("Height", DefaultDim::DIM_Y, t->mutable_inner_y_length());

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
      DrawRegionLengthEditor("Diameter", DefaultDim::DIM_X, t->mutable_diameter());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Inner diameter");
      ImGui::SameLine();
      bool use_inner = t->has_inner_diameter();
      ImGui::Checkbox("##InnerCheckbox", &use_inner);
      if (use_inner) {
        ImGui::Indent();
        DrawRegionLengthEditor("InnerDiameter", DefaultDim::DIM_X, t->mutable_inner_diameter());
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
      DrawRegionLengthEditor("XDiameter", DefaultDim::DIM_X, t->mutable_x_diameter());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Y diameter");
      ImGui::SameLine();
      DrawRegionLengthEditor("YDiameter", DefaultDim::DIM_Y, t->mutable_y_diameter());
    }

    if (support_depth) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Depth");
      ImGui::SameLine();
      DrawJitteredRegionLengthEditor(
          "Depth", DefaultDim::DIM_DEPTH, region->mutable_depth(), region->mutable_depth_jitter());
      ImGui::SameLine();
      ImGui::HelpMarker(
          "The distance away from the wall towards the camera. The greater the value, the further "
          "it is from the wall.");
    } else {
      region->clear_depth();
      region->clear_depth_jitter();
    }

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
          "XOffset", DefaultDim::DIM_X, region->mutable_x_offset(), /*is_point=*/true);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Y offset");
      ImGui::SameLine();
      DrawRegionLengthEditor(
          "YOffset", DefaultDim::DIM_Y, region->mutable_y_offset(), /*is_point=*/true);
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

        auto& profile = profile_list->at(number);
        if (profile.info().description().size() > 0) {
          ImGui::SameLine();
          ImGui::TextDisabled(profile.info().description().c_str());
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
      total_weight += p->info().weight();
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
      ImGui::InputText("##DescriptionInput", p->mutable_info()->mutable_description());
      ImGui::SameLine();
      if (ImGui::Button(kIconMoreVert)) {
        ImGui::OpenPopup(item_menu_id);
      }

      ImGui::Indent();
      if (use_weights) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Selection weight");
        ImGui::SameLine();
        int weight = p->info().weight();
        if (!p->info().has_weight()) {
          weight = 1;
        }
        ImGui::SetNextItemWidth(char_x_ * 10);
        ImGui::InputInt("##WeightInput", &weight, 1, 5);
        p->mutable_info()->set_weight(weight);

        if (total_weight > 0) {
          ImGui::SameLine();
          float weight_percent = (weight / total_weight) * 100;
          ImGui::TextDisabled("%.0f%%", weight_percent);
        }

        ImGui::InputInt(ImGui::InputIntParams("NextProfile")
                            .set_label("Next profile")
                            .set_step(1, 2)
                            .set_min(1)
                            .set_default(1)
                            .set_is_optional()
                            .set_width(char_x_ * 10),
                        PROTO_INT_FIELD(ProfileInfo, p->mutable_info(), next_profile));
        ImGui::SameLine();
        ImGui::HelpMarker("If this profile is selected, always select the specified profile next.");

        ImGui::InputInt(ImGui::InputIntParams("MinSelectionGap")
                            .set_label("Selection gap")
                            .set_step(1, 2)
                            .set_min(1)
                            .set_default(2)
                            .set_is_optional()
                            .set_width(char_x_ * 10),
                        PROTO_INT_FIELD(ProfileInfo, p->mutable_info(), min_selection_gap));
        ImGui::SameLine();
        ImGui::HelpMarker(
            "Limit how frequently the profile can be selected. A value of 2 means that 2 other "
            "profiles must be selected before this one can be chosen again.");

      } else {
        p->mutable_info()->clear_weight();
        p->mutable_info()->clear_next_profile();
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

  void DrawBoundsEditor(const std::string& id, Bounds* bounds, BoundsDimensions dimensions = {}) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Bounds");
    ImGui::Indent();

    if (dimensions.draw_width) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Width");
      ImGui::SameLine();
      bool use_width = bounds->has_width();
      ImGui::Checkbox("##WidthCheck", &use_width);
      if (use_width) {
        if (!bounds->has_width()) {
          bounds->mutable_width()->set_x_percent_value(0.9);
        }
        ImGui::SameLine();
        DrawRegionLengthEditor("Width", DefaultDim::DIM_X, bounds->mutable_width());
      } else {
        bounds->clear_width();
      }
    }

    if (dimensions.draw_height) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Height");
      ImGui::SameLine();
      bool use_height = bounds->has_height();
      ImGui::Checkbox("##HeightCheck", &use_height);
      if (use_height) {
        if (!bounds->has_height()) {
          bounds->mutable_height()->set_y_percent_value(0.9);
        }
        ImGui::SameLine();
        DrawRegionLengthEditor("Height", DefaultDim::DIM_Y, bounds->mutable_height());
      } else {
        bounds->clear_height();
      }
    }

    if (dimensions.draw_depth) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Depth");
      ImGui::SameLine();
      bool use_depth = bounds->has_depth();
      ImGui::Checkbox("##DepthCheck", &use_depth);
      if (use_depth) {
        if (!bounds->has_depth()) {
          bounds->mutable_depth()->set_depth_percent_value(0.5);
        }
        ImGui::SameLine();
        DrawRegionLengthEditor("Depth", DefaultDim::DIM_DEPTH, bounds->mutable_depth());
      } else {
        bounds->clear_depth();
      }
    }
    ImGui::Unindent();
  }

  enum class DefaultDim {
    DIM_X,
    DIM_Y,
    DIM_DEPTH,
  };

  Field<float> GetRegionLengthField(RegionLength* length, RegionLength::TypeCase type) {
    switch (type) {
      case RegionLength::kYPercentValue:
        return PROTO_PERCENT_FIELD(RegionLength, length, y_percent_value);
      case RegionLength::kDepthPercentValue:
        return PROTO_PERCENT_FIELD(RegionLength, length, depth_percent_value);
      case RegionLength::kXPercentValue:
        return PROTO_PERCENT_FIELD(RegionLength, length, x_percent_value);
      case RegionLength::kValue:
        break;
    }
    return PROTO_FLOAT_FIELD(RegionLength, length, value);
  }

  void DrawJitteredRegionLengthEditor(const std::string& id,
                                      DefaultDim default_dim,
                                      RegionLength* length,
                                      RegionLength* jitter) {
    ImGui::IdGuard cid(id);
    DrawRegionLengthEditor("RegionValue", default_dim, length);
    ImGui::Indent();
    ImGui::Text("+/-");
    ImGui::SameLine();
    DrawRegionLengthEditor("RegionJitterValue", default_dim, jitter, false, true);
    ImGui::Unindent();
  }

  void DrawOptionalRegionLengthEditor(const std::string& id,
                                      DefaultDim default_dim,
                                      PtrField<RegionLength> length,
                                      bool is_point = false,
                                      bool default_to_zero = false) {
    ImGui::IdGuard cid(id);
    bool has_value = length.has();
    ImGui::Checkbox("##UseRegionLength", &has_value);
    if (has_value) {
      ImGui::SameLine();
      DrawRegionLengthEditor(
          "RegionLength", default_dim, length.get_mutable(), is_point, default_to_zero);
    } else {
      length.clear();
    }
  }

  void DrawRegionLengthEditor(const std::string& id,
                              DefaultDim default_dim,
                              RegionLength* length,
                              bool is_point = false,
                              bool default_to_zero = false) {
    float default_percent = is_point || default_to_zero ? 0 : 0.50;
    ImGui::IdGuard cid(id);
    if (length->type_case() == RegionLength::TYPE_NOT_SET) {
      if (default_dim == DefaultDim::DIM_X) {
        length->set_x_percent_value(default_percent);
      } else if (default_dim == DefaultDim::DIM_Y) {
        length->set_y_percent_value(default_percent);
      } else {
        length->set_depth_percent_value(default_percent);
      }
    }

    bool is_percent = length->type_case() != RegionLength::kValue;
    Field<float> field = GetRegionLengthField(length, length->type_case());
    auto params = ImGui::InputFloatParams("ValueInput")
                      .set_step(1, 5)
                      .set_precision(0)
                      .set_width(char_x_ * 9);
    if (is_percent) {
      if (!is_point) {
        params.set_min(0);
      }
      params.set_default(100 * default_percent);
    }
    ImGui::InputFloat(params, field);

    const bool was_percent = is_percent;
    ImGui::SameLine();
    ImGui::Text("as percent");
    ImGui::SameLine();
    ImGui::Checkbox("##UsePercent", &is_percent);

    if (is_percent) {
      if (!was_percent) {
        if (default_dim == DefaultDim::DIM_X) {
          length->set_x_percent_value(default_percent);
        } else if (default_dim == DefaultDim::DIM_Y) {
          length->set_y_percent_value(default_percent);
        } else {
          length->set_depth_percent_value(default_percent);
        }
      }

      ImGui::SameLine();
      ImGui::Text("of");
      ImGui::SameLine();

      ImGui::PushItemWidth(char_x_ * 7);
      auto type = length->type_case();
      if (ImGui::SimpleTypeDropdown("##LengthType", &type, kRegionLengthTypes, char_x_ * 7)) {
        float old_value = glm::clamp<float>(field.get(), 0, 100);
        auto new_field = GetRegionLengthField(length, type);
        new_field.set(old_value);
      }

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
    DrawRegionLengthEditor("X" + id, DefaultDim::DIM_X, v->mutable_x(), /*is_point=*/true);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("y");
    ImGui::SameLine();
    DrawRegionLengthEditor("Y" + id, DefaultDim::DIM_Y, v->mutable_y(), /*is_point=*/true);
  }

  void DrawShotTypeEditor() {
    ImGui::IdGuard cid("ShotTypeEditor");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Shot type");
    ImGui::SameLine();

    auto type = def_.shot_type().type_case();
    ShotType& s = *def_.mutable_shot_type();

    if (ImGui::SimpleTypeDropdown("ShotTypeDropdown", &type, kShotTypes, char_x_ * 15)) {
      s.Clear();
      if (type == ShotType::kClickSingle) {
        s.set_click_single(true);
      }
      if (type == ShotType::kClickMulti) {
        s.set_click_multi(true);
        s.set_health_clicks(3);
      }
      if (type == ShotType::kTrackingInvincible) {
        s.set_tracking_invincible(true);
      }
      if (type == ShotType::kTrackingKill) {
        s.set_tracking_kill(true);
        s.set_health_seconds(0.4);
      }
      if (type == ShotType::kPoke) {
        s.set_poke(true);
      }
    }

    if (type == ShotType::kPoke) {
      ImGui::InputFloat(ImGui::InputFloatParams("PokeKillTime")
                            .set_label("Poke kill time")
                            .set_step(0.01, 0.1)
                            .set_min(0.01)
                            .set_precision(2)
                            .set_default(0.05)
                            .set_is_optional()
                            .set_width(char_x_ * 10),
                        PROTO_FLOAT_FIELD(ShotType, &s, poke_kill_time_seconds));
    }

    if (type == ShotType::kClickMulti) {
      ImGui::InputInt(ImGui::InputIntParams("ClickCount")
                          .set_label("Health clicks")
                          .set_step(1, 2)
                          .set_min(2)
                          .set_default(3)
                          .set_width(char_x_ * 10),
                      PROTO_INT_FIELD(ShotType, def_.mutable_shot_type(), health_clicks));
    }

    if (type == ShotType::kTrackingKill) {
      ImGui::InputFloat(ImGui::InputFloatParams("HealthSeconds")
                            .set_label("Health time")
                            .set_step(0.01, 0.1)
                            .set_min(0.01)
                            .set_precision(2)
                            .set_default(0.4)
                            .set_width(char_x_ * 10),
                        PROTO_FLOAT_FIELD(ShotType, &s, health_seconds));
      ImGui::SameLine();
      ImGui::HelpMarker("The amount of time in seconds to kill the target.");

      ImGui::InputFloat(ImGui::InputFloatParams("HealthRegenRate")
                            .set_label("Health regen rate")
                            .set_step(0.1, 0.5)
                            .set_min(0.1)
                            .set_precision(1)
                            .set_default(1)
                            .set_is_optional()
                            .set_width(char_x_ * 10),
                        PROTO_FLOAT_FIELD(ShotType, &s, health_regen_rate));
      ImGui::SameLine();
      ImGui::HelpMarker(
          "The rate health is regenerated if you switch off target before killing. 1 means regen "
          "at same rate as health is taken away for hits.");

      ImGui::InputFloat(ImGui::InputFloatParams("RemoveIfBelowHealthThreshold")
                            .set_label("Remove if below health percent")
                            .set_step(1, 5)
                            .set_min(15)
                            .set_max(99)
                            .set_precision(0)
                            .set_default(1)
                            .set_is_optional()
                            .set_width(char_x_ * 10),
                        PROTO_PERCENT_FIELD(ShotType, &s, remove_if_below_health_threshold));

      if (s.has_remove_if_below_health_threshold()) {
        ImGui::Indent();
        ImGui::InputFloat(ImGui::InputFloatParams("RemoveIfBelowHealthTime")
                              .set_label("After time")
                              .set_step(.01, .1)
                              .set_min(0)
                              .set_precision(2)
                              .set_default(.01)
                              .set_width(char_x_ * 10),
                          PROTO_FLOAT_FIELD(ShotType, &s, remove_if_below_health_time));
        ImGui::Unindent();
      } else {
        s.clear_remove_if_below_health_time();
      }

      ImGui::InputBool(ImGui::InputBoolParams("NoPartialKills")
                           .set_label("No partial kills")
                           .set_false_is_unset(),
                       PROTO_BOOL_FIELD(ShotType, def_.mutable_shot_type(), no_partial_kills));
    }
  }

  void VectorEditor(ImGui::InputFloatParams params, StoredVec3* v) {
    ImGui::IdGuard cid(params.id);

    ImGui::InputFloat(params.set_label("X").set_id("##XInput"),
                      PROTO_FLOAT_FIELD(StoredVec3, v, x));

    ImGui::InputFloat(params.set_label("Y").set_id("##YInput"),
                      PROTO_FLOAT_FIELD(StoredVec3, v, y));

    ImGui::InputFloat(params.set_label("Z").set_id("##ZInput"),
                      PROTO_FLOAT_FIELD(StoredVec3, v, z));
  }

  void DrawRoomEditor() {
    ImGui::IdGuard cid("RoomEditor");
    ImGui::SetNextWindowBgAlpha(0.4f);
    float width = char_x_ * 25;
    float height = app_.screen_info().height * 0.75;

    ImGui::SetNextWindowPos(ImVec2(char_x_ * 0.3, (app_.screen_info().height - height) / 2.0));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    if (!ImGui::Begin("Room", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
      ImGui::End();
      return;
    }

    if (ImGui::Button(std::format("{} Back to editor", kIconArrowBack))) {
      editing_room_ = false;
    }

    Line();

    Room& room = *def_.mutable_room();

    ImGuiComboFlags combo_flags = 0;

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
        ImGui::Indent();
        ImGui::SetNextItemWidth(char_x_ * 12);
        ImGui::InputFloat("##SideAngle", &side_angle, 1, 1, "%.0f");
        room.mutable_cylinder_room()->set_side_angle_degrees(side_angle);
        ImGui::Unindent();
      } else {
        room.mutable_cylinder_room()->clear_side_angle_degrees();
      }
    }

    Line();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Camera position");
    ImGui::Indent();
    VectorEditor(ImGui::InputFloatParams("CameraPositionVector")
                     .set_precision(0)
                     .set_step(1, 10)
                     .set_width(char_x_ * 10),
                 room.mutable_camera_position());
    ImGui::Unindent();

    ImGui::Spacing();
    bool has_camera_up = room.has_camera_up();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Set camera up");
    ImGui::SameLine();
    ImGui::Checkbox("##CameraUp", &has_camera_up);
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Define up for the camera (usually the z axis). This allows you to rotate the entire "
        "scenario. (1, 0, 1) would be a 45 degree rotation.");
    if (has_camera_up) {
      if (IsZero(room.camera_up())) {
        room.mutable_camera_up()->set_z(1);
      }
      ImGui::Indent();
      VectorEditor(ImGui::InputFloatParams("CameraUpVector")
                       .set_precision(1)
                       .set_step(0.1, 1)
                       .set_width(char_x_ * 10),
                   room.mutable_camera_up());
      ImGui::Unindent();
    } else {
      room.clear_camera_up();
    }

    ImGui::Spacing();
    bool has_camera_front = room.has_camera_front();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Set camera front");
    ImGui::SameLine();
    ImGui::Checkbox("##CameraFront", &has_camera_front);
    if (has_camera_front) {
      if (IsZero(room.camera_front())) {
        room.mutable_camera_front()->set_y(1);
      }
      ImGui::Indent();
      VectorEditor(ImGui::InputFloatParams("CameraFrontVector")
                       .set_precision(1)
                       .set_step(0.1, 1)
                       .set_width(char_x_ * 10),
                   room.mutable_camera_front());
      ImGui::Unindent();
    } else {
      room.clear_camera_front();
    }

    ImGui::End();
  }

  void DrawTargetEditor() {
    ImGui::IdGuard cid("TargetEditor");
    TargetDef* t = def_.mutable_target_def();

    bool is_single_target_tracking = VectorContains(kSingleTargetTrackingTypes, def_.type_case());
    if (is_single_target_tracking) {
      if (t->profiles_size() == 0) {
        t->add_profiles()->set_speed(40);
      }
      if (t->profiles_size() > 1) {
        TargetProfile first_profile = t->profiles(0);
        t->clear_profiles();
        *t->add_profiles() = first_profile;
      }

      TargetProfile p = t->profiles(0);

      t->Clear();
      *t->add_profiles() = p;
      t->set_num_targets(1);

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

    ImGui::InputFloat(ImGui::InputFloatParams("NewTargetDelaySeconds")
                          .set_label("New target delay")
                          .set_is_optional()
                          .set_zero_is_unset()
                          .set_step(0.05, 0.25)
                          .set_min(0.01)
                          .set_precision(2)
                          .set_default(0.2)
                          .set_width(char_x_ * 10),
                      PROTO_PERCENT_FIELD(TargetDef, t, new_target_delay_seconds));

    ImGui::InputFloat(ImGui::InputFloatParams("RemoveTargetAfterSeconds")
                          .set_label("Remove after time")
                          .set_is_optional()
                          .set_zero_is_unset()
                          .set_step(0.05, 0.25)
                          .set_min(0.01)
                          .set_precision(2)
                          .set_default(0.2)
                          .set_width(char_x_ * 10),
                      PROTO_PERCENT_FIELD(TargetDef, t, remove_target_after_seconds));

    ImGui::InputFloat(ImGui::InputFloatParams("StaggerInitialTargetsSeconds")
                          .set_label("Initial stagger time")
                          .set_is_optional()
                          .set_zero_is_unset()
                          .set_step(0.05, 0.25)
                          .set_min(0.01)
                          .set_precision(2)
                          .set_default(0.2)
                          .set_width(char_x_ * 10),
                      PROTO_PERCENT_FIELD(TargetDef, t, stagger_initial_targets_seconds));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Time in seconds between each target being added at the start of the scenario.");
  }

  void DrawTargetProfile(TargetProfile* profile) {
    ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Radius")
                                  .set_step(0.05, 0.5)
                                  .set_min(0.01)
                                  .set_precision(2)
                                  .set_default(2)
                                  .set_width(char_x_ * 10),
                              PROTO_JITTERED_FIELD(TargetProfile, profile, target_radius));

    ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Speed")
                                  .set_step(1, 10)
                                  .set_min(0)
                                  .set_precision(1)
                                  .set_zero_is_unset()
                                  .set_width(char_x_ * 10),
                              PROTO_JITTERED_FIELD(TargetProfile, profile, speed));

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Use pill shape");
    ImGui::SameLine();
    bool use_pill = profile->has_pill();
    ImGui::Checkbox("##UsePill", &use_pill);
    ImGui::SameLine();
    ImGui::HelpMarker("Switch from sphere target to a pill (capsule) shaped target.");
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

    if (def_.shot_type().type_case() == ShotType::kClickMulti ||
        def_.shot_type().type_case() == ShotType::kTrackingKill) {
      ImGui::InputFloat(ImGui::InputFloatParams("TargetRadiusAtill")
                            .set_label("Target radius at kill")
                            .set_step(0.1, 0.5)
                            .set_min(0.1)
                            .set_precision(1)
                            .set_default(profile->target_radius())
                            .set_is_optional()
                            .set_width(char_x_ * 10),
                        PROTO_FLOAT_FIELD(TargetProfile, profile, target_radius_at_kill));
      ImGui::SameLine();
      ImGui::HelpMarker(
          "The radius of the target will change to the specified value incrementally based on "
          "how much health remains");
    } else {
      profile->clear_target_radius_at_kill();
    }

    ImGui::InputFloat(ImGui::InputFloatParams("HitRadiusMultiplier")
                          .set_label("Hit radius multiplier")
                          .set_step(0.1, 0.5)
                          .set_min(0.1)
                          .set_precision(1)
                          .set_default(1)
                          .set_is_optional()
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(TargetProfile, profile, target_hit_radius_multiplier));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Updates the target's hit radius to not match the visuals. To make the hit box twice as "
        "large use a value of 2.");
  }

  void Render() override {
    if (!editing_room_) {
      UiScreen::Render();
      return;
    }

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
    if (def_.has_reference_def()) {
      auto base_scenario =
          app_.scenario_manager().GetEvaluatedScenario(def_.reference_def().scenario_id());
      if (!base_scenario) {
        SetErrorMessage(std::format("Unable to find referenced scenario \"{}\"",
                                    def_.reference_def().scenario_id()));
        return;
      }
      params.def = base_scenario->def;
      if (def_.has_overrides()) {
        *params.def.mutable_overrides() = def_.overrides();
      }
    } else {
      params.def = def_;
    }
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

  std::string add_to_playlist_;

  ImGui::NotificationPopup notification_popup_{"Notification"};

  bool editing_room_ = false;
  bool editing_description_ = false;
};

}  // namespace

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(const ScenarioEditorOptions& options,
                                                     Application* app) {
  return std::make_unique<ScenarioEditorScreen>(options, *app);
}

}  // namespace aim

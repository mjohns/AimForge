#include "scenario_editor.h"

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
#include "aim/core/camera.h"
#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"
#include "aim/scenario/scenario.h"

namespace aim {
namespace {

const char* kErrorPopup = "ERROR_POPUP";

const std::vector<std::pair<Room::TypeCase, std::string>> kRoomTypes{
    {Room::kSimpleRoom, "Simple"},
    {Room::kCylinderRoom, "Cylinder"},
    {Room::kBarrelRoom, "Barrel"},
};

const std::vector<std::pair<InOutDirection, std::string>> kInOutDirections{
    {InOutDirection::IN, "Towards center"},
    {InOutDirection::OUT, "Away from center"},
    {InOutDirection::RANDOM, "Random"},
};

const std::vector<std::pair<ShotType::TypeCase, std::string>> kShotTypes{
    {ShotType::kClickSingle, "Single"},
    {ShotType::kTrackingInvincible, "Tracking"},
    {ShotType::kTrackingKill, "Tracking kill"},
    {ShotType::kPoke, "Poke"},
    {ShotType::TYPE_NOT_SET, "Default"},
};

const std::vector<std::pair<ScenarioDef::TypeCase, std::string>> kScenarioTypes{
    {ScenarioDef::kStaticDef, "Static"},
    {ScenarioDef::kCenteringDef, "Centering"},
    {ScenarioDef::kWallStrafeDef, "Wall Strafe"},
    {ScenarioDef::kBarrelDef, "Barrel"},
    {ScenarioDef::kLinearDef, "Linear"},
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
  explicit ScenarioEditorScreen(const ScenarioEditorOptions& opts, Application* app)
      : UiScreen(app), target_manager_(GetDefaultSimpleRoom()) {
    projection_ = GetPerspectiveTransformation(app_->screen_info());
    auto themes = app_->settings_manager()->ListThemes();
    if (themes.size() > 0) {
      theme_ = app_->settings_manager()->GetTheme(themes[0]);
    } else {
      theme_ = GetDefaultTheme();
    }
    settings_ = app_->settings_manager()->GetCurrentSettings();
    *def_.mutable_room() = GetDefaultSimpleRoom();
    bundle_names_ = app->file_system()->GetBundleNames();

    auto initial_scenario = app_->scenario_manager()->GetScenario(opts.scenario_id);
    if (initial_scenario.has_value()) {
      def_ = initial_scenario->def;
      name_ = initial_scenario->name;
      if (opts.is_new_copy) {
        std::string final_name = MakeUniqueName(
            name_.relative_name() + " Copy",
            app_->scenario_manager()->GetAllRelativeNamesInBundle(name_.bundle_name()));
        *name_.mutable_relative_name() = final_name;
      } else {
        original_name_ = initial_scenario->name;
      }
    }
  }

 protected:
  void DrawScreen() override {
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_size_ = char_size;
    char_x_ = char_size_.x;

    ImGui::SimpleDropdown("BundlePicker", name_.mutable_bundle_name(), bundle_names_, char_x_ * 7);

    ImGui::PushItemWidth(char_x_ * 7);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 40);
    ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());

    float duration_seconds = FirstGreaterThanZero(def_.duration_seconds(), 60);
    ImGui::Text("Duration");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##DurationSeconds", &duration_seconds, 15, 1, "%.0f");
    def_.set_duration_seconds(duration_seconds);

    if (ImGui::TreeNodeEx("Scenario", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Indent();
      DrawScenarioTypeEditor(char_size);
      ImGui::Unindent();
      ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Targets", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Indent();
      DrawTargetEditor(char_size);
      ImGui::Unindent();
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Room")) {
      ImGui::Indent();
      DrawRoomEditor(char_size);
      ImGui::Unindent();
      ImGui::TreePop();
    }

    if (ImGui::Button("Play")) {
      start_scenario_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("View Json")) {
      SetErrorMessage(MessageToJson(def_, 6));
    }

    if (ImGui::Button("Save", ImVec2(char_x_ * 14, 0))) {
      if (SaveScenario()) {
        app_->scenario_manager()->LoadScenariosFromDisk();
        ScreenDone();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ScreenDone();
    }

    bool show_error_popup = error_popup_message_.size() > 0;
    if (show_error_popup) {
      ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                              ImGuiCond_Appearing,
                              ImVec2(0.5f, 0.5f));  // Center the popup
      if (ImGui::BeginPopupModal(kErrorPopup,
                                 &show_error_popup,
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::Text(error_popup_message_);

        float button_width = ImGui::CalcTextSize("OK").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - button_width) * 0.5f);

        if (ImGui::Button("OK", ImVec2(button_width, 0))) {
          error_popup_message_ = "";
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
  }

  // Returns whether the screen should close
  bool SaveScenario() {
    if (name_.bundle_name().size() == 0 || name_.relative_name().size() == 0) {
      SetErrorMessage("Missing scenario name");
      return false;
    }

    auto& mgr = *app_->scenario_manager();

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

    return true;
  }

  void SetErrorMessage(const std::string& msg) {
    error_popup_message_ = msg;
    ImGui::OpenPopup(kErrorPopup);
  }

  void DrawScenarioTypeEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("ScenarioTypeEditor");
    ImGui::Text("Scenario type");
    ImGui::SameLine();

    if (def_.type_case() == ScenarioDef::TYPE_NOT_SET) {
      def_.mutable_static_def();
    }

    auto scenario_type = def_.type_case();
    ImGui::SimpleTypeDropdown("ScenarioTypeDropdown", &scenario_type, kScenarioTypes, char_x_ * 15);

    DrawShotTypeEditor(char_size);

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
  }

  void DrawLinearEditor() {
    ImGui::IdGuard cid("LinearEditor");
    LinearScenarioDef& d = *def_.mutable_linear_def();

    ImGui::Text("Angle");
    ImGui::SameLine();
    float angle = d.angle();
    float angle_jitter = d.angle_jitter();
    JitteredValueInput("AngleInput", &angle, &angle_jitter, 1, 3, "%.0f");
    d.set_angle(angle);
    d.set_angle_jitter(angle_jitter);

    ImGui::Text("Initial direction");
    ImGui::SameLine();
    InOutDirection direction_type = d.has_direction() ? d.direction() : InOutDirection::IN;
    ImGui::SimpleTypeDropdown(
        "DirectionTypeDropdown", &direction_type, kInOutDirections, char_x_ * 20);
    d.set_direction(direction_type);

    ImGui::Text("Initial target location");
    ImGui::Indent();
    DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
    ImGui::Unindent();

    if (ImGui::TreeNode("Advanced")) {
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
      ImGui::TreePop();
    }
  }

  void DrawBarrelEditor() {
    ImGui::IdGuard cid("BarrelEditor");
    BarrelScenarioDef& d = *def_.mutable_barrel_def();

    if (!def_.room().has_barrel_room()) {
      ImGui::Text("Must use barrel room");
      return;
    }

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

    ImGui::Text("Strafing bounds");
    ImGui::Indent();
    ImGui::Text("Width");
    ImGui::SameLine();
    DrawRegionLengthEditor("Width", /*default_to_x=*/true, w.mutable_width());
    ImGui::Text("Height");
    ImGui::SameLine();
    DrawRegionLengthEditor("Height", /*default_to_x=*/false, w.mutable_height());
    ImGui::Unindent();

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

    DrawProfileList("ProfileList",
                    "Profile",
                    w.mutable_profile_order(),
                    w.mutable_profiles(),
                    std::bind_front(&ScenarioEditorScreen::DrawWallStrafeProfile, this));

    ImGui::Spacing();

    float acceleration = w.acceleration();
    ImGui::Text("Acceleration");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##AccelerationInput", &acceleration, 1, 5, "%.0f");
    if (acceleration <= 0) {
      w.clear_acceleration();
      w.clear_deceleration();
    } else {
      w.set_acceleration(acceleration);

      bool has_decel = w.deceleration() > 0;
      float deceleration = w.deceleration();
      ImGui::Text("Different deceleration");
      ImGui::SameLine();
      OptionalInputFloat("##DecelerationInput", &has_decel, &deceleration, 1, 5, "%.0f");
      if (has_decel) {
        w.set_deceleration(ClampPositive(deceleration));
      } else {
        w.clear_deceleration();
      }
    }
  }

  void DrawWallStrafeProfile(WallStrafeProfile* p) {
    ImGui::Text("Min distance");
    ImGui::SameLine();
    DrawRegionLengthEditor("MinDistance", /*default_to_x=*/true, p->mutable_min_distance());

    ImGui::Text("Max distance");
    ImGui::SameLine();
    DrawRegionLengthEditor("MaxDistance", /*default_to_x=*/true, p->mutable_max_distance());
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

    ImGui::Text("Point 1");
    ImGui::Indent();
    DrawRegionVec2Editor("Point1", c.mutable_wall_points(0));
    ImGui::Unindent();

    ImGui::Text("Point 2");
    ImGui::Indent();
    DrawRegionVec2Editor("Point2", c.mutable_wall_points(1));
    ImGui::Unindent();

    ImGui::Text("Orient pill");
    ImGui::SameLine();
    bool orient_pill = c.orient_pill();
    ImGui::Checkbox("##OrientPillCheck", &orient_pill);
    if (orient_pill) {
      c.set_orient_pill(true);
    } else {
      c.clear_orient_pill();
    }
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

    DrawProfileList("RegionList",
                    "Region",
                    s->mutable_region_order(),
                    s->mutable_regions(),
                    std::bind_front(&ScenarioEditorScreen::DrawTargetRegion, this));

    ImGui::Spacing();
    ImGui::Text("Min distance between targets");
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

    ImGui::Text("Fixed distance from last target");
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
  }

  void DrawTargetRegion(TargetRegion* region) {
    if (region->type_case() == TargetRegion::TYPE_NOT_SET) {
      region->mutable_rectangle();
    }
    auto region_type = region->type_case();
    ImGui::SimpleTypeDropdown("RegionTypeDropdown", &region_type, kRegionTypes, char_x_ * 15);

    if (region_type == TargetRegion::kRectangle) {
      auto* t = region->mutable_rectangle();
      ImGui::Text("Width");
      ImGui::SameLine();
      DrawRegionLengthEditor("Width", /*default_to_x=*/true, t->mutable_x_length());

      ImGui::Text("Height");
      ImGui::SameLine();
      DrawRegionLengthEditor("Height", /*default_to_x=*/false, t->mutable_y_length());

      ImGui::Text("Inner");
      ImGui::SameLine();
      bool use_inner = t->has_inner_x_length() || t->has_inner_y_length();
      ImGui::Checkbox("##InnerCheckbox", &use_inner);
      if (use_inner) {
        ImGui::IdGuard lid("InnerInputs");
        ImGui::Indent();

        ImGui::Text("Width");
        ImGui::SameLine();
        DrawRegionLengthEditor("Width", /*default_to_x=*/true, t->mutable_inner_x_length());

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
      ImGui::Text("Diameter");
      ImGui::SameLine();
      DrawRegionLengthEditor("Diameter", /*default_to_x=*/true, t->mutable_diameter());

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
      ImGui::Text("X diameter");
      ImGui::SameLine();
      DrawRegionLengthEditor("XDiameter", /*default_to_x=*/true, t->mutable_x_diameter());

      ImGui::Text("Y diameter");
      ImGui::SameLine();
      DrawRegionLengthEditor("YDiameter", /*default_to_x=*/false, t->mutable_y_diameter());
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Depth");
    ImGui::SameLine();
    float depth = region->depth();
    float depth_jitter = region->depth_jitter();
    JitteredValueInput("DepthInput", &depth, &depth_jitter, 1, 5, "%.0f");
    region->set_depth(depth);
    region->set_depth_jitter(depth_jitter);

    ImGui::Text("Offset");
    ImGui::SameLine();
    bool use_offsets = region->has_x_offset() || region->has_y_offset();
    ImGui::Checkbox("##OffsetsCheckbox", &use_offsets);
    if (use_offsets) {
      ImGui::Indent();
      ImGui::Text("X offset");
      ImGui::SameLine();
      DrawRegionLengthEditor(
          "XOffset", /*default_to_x=*/true, region->mutable_x_offset(), /*is_point=*/true);

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
    ImGui::TextFmt("Explicit {} selection order", lower_type_name);
    bool use_order = order_list->size() > 0;
    ImGui::SameLine();
    ImGui::Checkbox("##UseOrder", &use_order);
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
      std::string icon = "\xEE\x97\x89";
      if (ImGui::Button(std::format("Add{}##Order", icon).c_str())) {
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
    for (int i = 0; i < profile_list->size(); ++i) {
      ImGui::IdGuard lid(type_name, i);
      auto* p = &profile_list->at(i);
      ImGui::TextFmt("{} #{}", type_name, i);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(char_x_ * 22);
      ImGui::InputText("##DescriptionInput", p->mutable_description());
      if (ImGui::BeginPopupContextItem("profile_item_menu")) {
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
      ImGui::OpenPopupOnItemClick("profile_item_menu", ImGuiPopupFlags_MouseButtonRight);

      ImGui::Indent();
      if (use_weights) {
        ImGui::Text("Selection weight");
        ImGui::SameLine();
        int weight = p->weight();
        if (!p->has_weight()) {
          weight = 1;
        }
        ImGui::SetNextItemWidth(char_x_ * 10);
        ImGui::InputInt("##WeightInput", &weight, 1, 5);
        p->set_weight(weight);
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

    ImGui::SetNextItemWidth(char_x_ * 12);
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

      ImGui::PushItemWidth(char_x_ * 12);
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
      // TODO: Display evaluated value based on room.
    } else {
      if (!length->has_value()) {
        length->set_value(0);
      }
    }
  }

  void DrawRegionVec2Editor(const std::string& id, RegionVec2* v) {
    ImGui::IdGuard cid(id);
    ImGui::Text("x");
    ImGui::SameLine();
    DrawRegionLengthEditor("X" + id, /*default_to_x=*/true, v->mutable_x(), /*is_point=*/true);
    ImGui::Text("y");
    ImGui::SameLine();
    DrawRegionLengthEditor("Y" + id, /*default_to_x=*/false, v->mutable_y(), /*is_point=*/true);
  }

  void DrawShotTypeEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("ShotTypeEditor");
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
      ImGui::Text("Width");
      ImGui::SameLine();
      float width = room.simple_room().width();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomWidth", &width, 10, 1, "%.0f");
      room.mutable_simple_room()->set_width(width);

      ImGui::Text("Height");
      ImGui::SameLine();
      float height = room.simple_room().height();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomHeight", &height, 10, 1, "%.0f");
      room.mutable_simple_room()->set_height(height);
    }

    if (room.type_case() == Room::kBarrelRoom) {
      ImGui::Text("Radius");
      ImGui::SameLine();
      float radius = room.barrel_room().radius();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomRadius", &radius, 5, 1, "%.0f");
      room.mutable_barrel_room()->set_radius(radius);
    }

    if (room.type_case() == Room::kCylinderRoom) {
      ImGui::Text("Height");
      ImGui::SameLine();
      float height = room.cylinder_room().height();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomHeight", &height, 10, 1, "%.0f");

      bool use_width_percent = room.cylinder_room().width_perimeter_percent() > 0;
      ImGui::Text("Width as percent of perimeter?");
      ImGui::SameLine();
      ImGui::Checkbox("##WidthPercentCheckbox", &use_width_percent);

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
      ImGui::Text("Radius");
      ImGui::SameLine();
      float radius = room.cylinder_room().radius();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomRadius", &radius, 10, 1, "%.0f");
      room.mutable_cylinder_room()->set_radius(radius);

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
        ImGui::Text("Side angle degrees");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(char_x_ * 12);
        ImGui::InputFloat("##SideAngle", &side_angle, 1, 1, "%.0f");
        room.mutable_cylinder_room()->set_side_angle_degrees(side_angle);
      } else {
        room.mutable_cylinder_room()->clear_side_angle_degrees();
      }
    }

    ImGui::Text("Camera position");
    ImGui::SameLine();
    VectorEditor("CameraPositionVector", room.mutable_camera_position(), char_size);

    if (ImGui::TreeNode("Advanced")) {
      bool has_camera_up = room.has_camera_up();
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

      bool has_camera_front = room.has_camera_front();
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

      ImGui::TreePop();
    }
  }

  void DrawTargetEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("TargetEditor");
    TargetDef* t = def_.mutable_target_def();

    int num_targets = t->num_targets();
    if (num_targets <= 0) {
      num_targets = 1;
    }
    ImGui::Text("Number");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 8);
    ImGui::InputInt("##NumberEntry", &num_targets, 1, 1);
    t->set_num_targets(num_targets);

    ImGui::Text("Remove closest target on miss");
    ImGui::SameLine();
    bool remove_closest = t->remove_closest_on_miss();
    ImGui::Checkbox("##RemoveClosest", &remove_closest);
    t->set_remove_closest_on_miss(remove_closest);

    ImGui::Text("Newest target is ghost");
    ImGui::SameLine();
    bool is_ghost = t->newest_target_is_ghost();
    ImGui::Checkbox("##IsGhost", &is_ghost);
    t->set_newest_target_is_ghost(is_ghost);

    if (t->profiles_size() == 0) {
      t->add_profiles();
    }

    DrawProfileList("ProfileList",
                    "Profile",
                    t->mutable_target_order(),
                    t->mutable_profiles(),
                    std::bind_front(&ScenarioEditorScreen::DrawTargetProfile, this));

    if (ImGui::TreeNode("Advanced")) {
      ImGui::Text("New target delay seconds");
      ImGui::SameLine();
      float new_target_delay = t->new_target_delay_seconds();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##NewTargetDelay", &new_target_delay, 0.1, 0.1, "%.2f");
      t->set_new_target_delay_seconds(new_target_delay);

      ImGui::Text("Remove target after seconds");
      ImGui::SameLine();
      float remove_after = t->remove_target_after_seconds();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RemoveAfterDelay", &remove_after, 0.1, 0.1, "%.2f");
      t->set_remove_target_after_seconds(remove_after);

      ImGui::Text("Stagger initial targets seconds");
      ImGui::SameLine();
      float stagger = t->stagger_initial_targets_seconds();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##StaggerDelay", &stagger, 0.1, 0.1, "%.2f");
      t->set_stagger_initial_targets_seconds(stagger);

      ImGui::TreePop();
    }
  }

  void DrawTargetProfile(TargetProfile* profile) {
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
    ImGui::Text("Pulse");
    ImGui::SameLine();
    ImGui::Checkbox("##PulseCheckbox", &has_growth);
    ImGui::SameLine();
    ImGui::TextDisabled(kIconHelp);
    if (ImGui::BeginItemTooltip()) {
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
      ImGui::Text(
          "Target will grow/shrink to a certain size over some duration. If it is not killed by "
          "then, it will be removed");
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
    if (has_growth) {
      ImGui::Indent();
      ImGui::Text("Time seconds");
      ImGui::SameLine();
      float growth_time = FirstGreaterThanZero(profile->target_radius_growth_time_seconds(), 2);
      ImGui::SetNextItemWidth(char_x_ * 10);
      ImGui::InputFloat("##GrowthTime", &growth_time, 0.1, 0.5, "%.1f");
      profile->set_target_radius_growth_time_seconds(std::max(growth_time, 0.1f));

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

    ImGui::Text("Pill");
    ImGui::SameLine();
    bool use_pill = profile->has_pill();
    ImGui::Checkbox("##UsePill", &use_pill);
    if (use_pill) {
      ImGui::Indent();

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
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##ValueEntry", value, step, fast_step, format);

    ImGui::SameLine();
    ImGui::Text("+/-");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 12);
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
    if (app_->StartRender(&ctx)) {
      app_->renderer()->DrawScenario(projection_,
                                     def_.room(),
                                     theme_,
                                     settings_.health_bar(),
                                     target_manager_.GetTargets(),
                                     look_at,
                                     &ctx,
                                     stopwatch,
                                     &frame_times);
      app_->FinishRender(&ctx);
    }
  }

  void OnTickStart() override {
    if (!start_scenario_) {
      return;
    }
    start_scenario_ = false;
    CreateScenarioParams params;
    params.def = def_;
    params.def.set_duration_seconds(1000000);
    params.id = name_.full_name();
    params.force_start_immediately = true;
    CreateScenario(params, app_)->Run();
    app_->EnableVsync();
    SDL_SetWindowRelativeMouseMode(app_->sdl_window(), false);
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (user_is_typing) {
      return;
    }
    if (IsMappableKeyDownEvent(event)) {
      std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
      if (KeyMappingMatchesEvent(
              event_name,
              app_->settings_manager()->GetCurrentSettings().keybinds().restart_scenario())) {
        start_scenario_ = true;
      }
    }
  }

 private:
  ScenarioDef def_;
  TargetManager target_manager_;
  glm::mat4 projection_;
  Theme theme_;
  float char_x_ = 0;
  ImVec2 char_size_{};

  bool start_scenario_ = false;
  std::vector<std::string> bundle_names_;
  std::optional<ResourceName> original_name_;
  ResourceName name_;
  Settings settings_;

  std::string error_popup_message_;
};

}  // namespace

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(const ScenarioEditorOptions& options,
                                                     Application* app) {
  return std::make_unique<ScenarioEditorScreen>(options, app);
}

}  // namespace aim

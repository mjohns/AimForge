#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/common/util.h"
#include "aim/core/camera.h"
#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"
#include "theme_editor_screen.h"

namespace aim {
namespace {

const std::vector<Room::TypeCase> kSupportedRoomTypes{
    Room::kSimpleRoom,
    Room::kCylinderRoom,
    Room::kBarrelRoom,
};
const std::vector<ShotType::TypeCase> kShotTypes{
    ShotType::kClickSingle,
    ShotType::kTrackingInvincible,
    ShotType::kTrackingKill,
    ShotType::kPoke,
    ShotType::TYPE_NOT_SET,
};
const std::vector<ScenarioDef::TypeCase> kScenarioTypes{
    ScenarioDef::kStaticDef,
    ScenarioDef::kCenteringDef,
};
const std::vector<TargetRegion::TypeCase> kRegionTypes{
    TargetRegion::kRectangle,
    TargetRegion::kCircle,
    TargetRegion::kEllipse,
};

std::string RegionTypeToString(const TargetRegion::TypeCase& type) {
  switch (type) {
    case TargetRegion::kRectangle:
      return "Rectangle";
    case TargetRegion::kCircle:
      return "Circle";
    case TargetRegion::kEllipse:
      return "Ellipse";
    default:
      break;
  }
  return "";
}

std::string ShotTypeToString(const ShotType::TypeCase& type) {
  switch (type) {
    case ShotType::kClickSingle:
      return "Click";
    case ShotType::kTrackingInvincible:
      return "Tracking";
    case ShotType::kTrackingKill:
      return "Tracking kill";
    case ShotType::kPoke:
      return "Poke";
    case ShotType::TYPE_NOT_SET:
      return "Default";
    default:
      break;
  }
  return "";
}

std::string RoomTypeToString(const Room::TypeCase& type) {
  switch (type) {
    case Room::kSimpleRoom:
      return "Simple Room";
    case Room::kCylinderRoom:
      return "Cylinder Room";
    case Room::kBarrelRoom:
      return "Barrel Room";
    default:
      break;
  }
  return "";
}

std::string ScenarioTypeToString(const ScenarioDef::TypeCase& type) {
  switch (type) {
    case ScenarioDef::kStaticDef:
      return "Static";
    case ScenarioDef::kCenteringDef:
      return "Centering";
    case ScenarioDef::kBarrelDef:
      return "Barrel";
    case ScenarioDef::kLinearDef:
      return "Linear";
    default:
      break;
  }
  return "";
}

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
  explicit ScenarioEditorScreen(Application* app)
      : UiScreen(app), room_(GetDefaultSimpleRoom()), target_manager_(room_) {
    projection_ = GetPerspectiveTransformation(app_->screen_info());
    auto themes = app_->settings_manager()->ListThemes();
    if (themes.size() > 0) {
      theme_ = app_->settings_manager()->GetTheme(themes[0]);
    } else {
      theme_ = GetDefaultTheme();
    }
  }

 protected:
  void DrawScreen() override {
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_size_ = char_size;
    char_x_ = char_size_.x;

    float duration_seconds = def_.duration_seconds();
    if (duration_seconds <= 0) {
      duration_seconds = 60;
    }
    ImGui::Text("Duration");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##DurationSeconds", &duration_seconds, 15, 1, "%.0f");
    def_.set_duration_seconds(duration_seconds);

    if (ImGui::TreeNode("Room")) {
      ImGui::Indent();
      DrawRoomEditor(char_size);
      ImGui::Unindent();
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Targets")) {
      ImGui::Indent();
      DrawTargetEditor(char_size);
      ImGui::Unindent();
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Scenario")) {
      ImGui::Indent();
      DrawScenarioTypeEditor(char_size);
      ImGui::Unindent();
      ImGui::TreePop();
    }

    {
      ImVec2 sz = ImVec2(char_x_ * 14, 0.0f);
      if (ImGui::Button("Save", sz)) {
        // app_->settings_manager()->SaveScenarioToDisk(current_theme_name_, current_theme_);
        ScreenDone();
      }
    }
    {
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        ScreenDone();
      }
    }

    /*
    Crosshair crosshair;
    crosshair.mutable_dot()->set_draw_outline(true);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    DrawCrosshair(crosshair, 25, current_theme_, app_->screen_info(), draw_list);
    */
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {}

  void DrawScenarioTypeEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("ScenarioTypeEditor");
    ImGui::Text("Scenario type");
    ImGui::SameLine();

    if (def_.type_case() == ScenarioDef::TYPE_NOT_SET) {
      def_.mutable_static_def();
    }

    ImGui::PushItemWidth(char_x_ * 25);
    auto scenario_type = def_.type_case();
    std::string type_string = ScenarioTypeToString(scenario_type);
    if (ImGui::BeginCombo("##ScenarioTypeCombo", type_string.c_str(), 0)) {
      ImGui::LoopId loop_id;
      for (auto type_item : kScenarioTypes) {
        auto lid = loop_id.Get();
        std::string item_name = ScenarioTypeToString(type_item);
        bool is_selected = type_string == item_name;
        if (ImGui::Selectable(item_name.c_str(), is_selected)) {
          scenario_type = type_item;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    DrawShotTypeEditor(char_size);

    if (scenario_type == ScenarioDef::kStaticDef) {
      DrawStaticEditor();
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

    ImGui::Text("Explicit region selection order");
    bool use_order = s->region_order_size() > 0;
    ImGui::SameLine();
    ImGui::Checkbox("##UseOrder", &use_order);
    if (use_order) {
      ImGui::Indent();
      if (s->region_order_size() == 0) {
        s->add_region_order(0);
      }
      int remove_at_i = -1;
      for (int i = 0; i < s->region_order_size(); ++i) {
        ImGui::IdGuard lid("RegionOrder", i);
        u32 region_number = s->region_order(i);
        u32 step = 1;
        ImGui::Text("Region");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(char_x_ * 8);
        ImGui::InputScalar(
            "##OrderItemInput", ImGuiDataType_U32, &region_number, &step, nullptr, "%u");
        region_number = std::min<u32>(region_number, s->regions_size() - 1);
        s->set_region_order(i, region_number);

        ImGui::SameLine();
        if (ImGui::Button("x")) {
          remove_at_i = i;
        }
      }
      if (ImGui::Button("Add##RegionOrder")) {
        s->add_region_order(0);
      }
      if (remove_at_i >= 0) {
        s->mutable_region_order()->erase(s->mutable_region_order()->begin() + remove_at_i);
      }
      ImGui::Unindent();
    } else {
      s->clear_region_order();
    }

    bool allow_percents = s->region_order_size() == 0 && s->regions_size() > 1;
    for (int i = 0; i < s->regions_size(); ++i) {
      ImGui::IdGuard lid("Region", i);
      ImGui::Text("Region #%d", i);
      ImGui::Indent();
      DrawTargetRegion(s->mutable_regions(i), allow_percents);
      ImGui::Unindent();
    }

    if (ImGui::Button("Add region")) {
      s->add_regions();
    }

    ImGui::Spacing();
    ImGui::Text("Min distance between targets");
    ImGui::SameLine();
    bool use_min = s->has_min_distance();
    ImGui::Checkbox("##MinDistanceCheck", &use_min);
    if (use_min) {
      float value = s->min_distance();
      if (!s->has_min_distance()) {
        value = 20;
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##MinDistance", &value, 1, 10, "%.0f");
      if (value < 0) {
        value = 0;
      }
      s->set_min_distance(value);
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
      ImGui::SameLine();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##FixedDistance", &value, 1, 10, "%.0f");
      if (value < 0) {
        value = 0;
      }
      s->set_fixed_distance_from_last_target(value);

      ImGui::SameLine();
      ImGui::Text("+/-");
      ImGui::SameLine();
      float jitter = s->fixed_distance_jitter();
      ImGui::SetNextItemWidth(char_x_ * 10);
      ImGui::InputFloat("##FixedDistanceJitter", &jitter, 0.5, 1, "%.1f");
      if (jitter < 0) {
        jitter = 0;
      }
      s->set_fixed_distance_jitter(jitter);
    } else {
      s->clear_fixed_distance_from_last_target();
      s->clear_fixed_distance_jitter();
    }
  }

  void DrawTargetRegion(TargetRegion* region, bool allow_percents) {
    if (region->type_case() == TargetRegion::TYPE_NOT_SET) {
      region->mutable_rectangle();
    }
    if (allow_percents) {
      ImGui::Text("Percent chance to use");
      ImGui::SameLine();
      int percent = region->percent_chance() * 100;
      if (percent <= 0) {
        percent = 100;
      }
      ImGui::SetNextItemWidth(char_x_ * 10);
      ImGui::InputInt("##PercentChance", &percent, 5, 10);
      region->set_percent_chance(percent / 100.0);
    } else {
      region->clear_percent_chance();
    }

    ImGui::PushItemWidth(char_x_ * 25);
    auto region_type = region->type_case();
    std::string type_string = RegionTypeToString(region_type);
    if (ImGui::BeginCombo("##RegionTypeCombo", type_string.c_str(), 0)) {
      ImGui::LoopId loop_id;
      for (auto type_item : kRegionTypes) {
        auto lid = loop_id.Get();
        std::string item_name = RegionTypeToString(type_item);
        bool is_selected = type_string == item_name;
        if (ImGui::Selectable(item_name.c_str(), is_selected)) {
          region_type = type_item;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

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
      DrawRegionLengthEditor("XDiameter", /*default_to_x=*/false, t->mutable_y_diameter());
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Offset");
    ImGui::SameLine();
    bool use_offsets = region->has_x_offset() && region->has_y_offset();
    ImGui::Checkbox("##OffsetsCheckbox", &use_offsets);
    if (use_offsets) {
      ImGui::Indent();
      ImGui::Text("X offset");
      ImGui::SameLine();
      DrawRegionLengthEditor("XOffset", /*default_to_x=*/true, region->mutable_x_offset());

      ImGui::Text("Y offset");
      ImGui::SameLine();
      DrawRegionLengthEditor("YOffset", /*default_to_x=*/false, region->mutable_y_offset());
      ImGui::Unindent();
    } else {
      region->clear_x_offset();
      region->clear_y_offset();
    }
  }

  void DrawRegionLengthEditor(const std::string& id, bool default_to_x, RegionLength* length) {
    ImGui::IdGuard cid(id);
    bool is_x = length->type_case() == RegionLength::kXPercentValue;
    bool is_y = length->type_case() == RegionLength::kYPercentValue;
    bool is_percent = is_x || is_y;

    ImGui::SetNextItemWidth(char_x_ * 12);
    if (is_percent) {
      float value = is_x ? length->x_percent_value() : length->y_percent_value();
      value *= 100;
      if (value <= 0) {
        value = 50;
      }
      ImGui::InputFloat("##PercentValue", &value, 5, 10, "%.0f");

      value /= 100.0;
      if (is_x) {
        length->set_x_percent_value(value);
      } else {
        length->set_y_percent_value(value);
      }
    } else {
      // Absolute value.
      float value = length->value();
      if (value <= 0) {
        value = 50;
      }
      ImGui::InputFloat("##AbsoluteValue", &value, 10, 20, "%.0f");
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
    }
  }

  void DrawShotTypeEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("ShotTypeEditor");
    ImGui::Text("Shot type");
    ImGui::SameLine();

    ImGui::PushItemWidth(char_x_ * 25);
    std::string type_string = ShotTypeToString(def_.shot_type().type_case());
    if (ImGui::BeginCombo("##shot_type_combo", type_string.c_str(), 0)) {
      ImGui::LoopId loop_id;
      for (auto type : kShotTypes) {
        auto lid = loop_id.Get();
        std::string name = ShotTypeToString(type);
        bool is_selected = type_string == name;
        if (ImGui::Selectable(name.c_str(), is_selected)) {
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
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
  }

  void DrawRoomEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("RoomEditor");

    ImGuiComboFlags combo_flags = 0;
    if (room_.type_case() == Room::TYPE_NOT_SET) {
      room_ = GetDefaultSimpleRoom();
    }

    ImGui::PushItemWidth(char_x_ * 25);
    std::string room_type_string = RoomTypeToString(room_.type_case());
    if (ImGui::BeginCombo("##room_type_combo", room_type_string.c_str(), combo_flags)) {
      ImGui::LoopId loop_id;
      for (auto type : kSupportedRoomTypes) {
        auto lid = loop_id.Get();
        std::string name = RoomTypeToString(type);
        bool is_selected = room_type_string == name;
        if (ImGui::Selectable(name.c_str(), is_selected)) {
          if (type != room_.type_case()) {
            if (type == Room::kSimpleRoom) {
              room_ = GetDefaultSimpleRoom();
            }
            if (type == Room::kCylinderRoom) {
              room_ = GetDefaultCylinderRoom();
            }
            if (type == Room::kBarrelRoom) {
              room_ = GetDefaultBarrelRoom();
            }
          }
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    if (room_.type_case() == Room::kSimpleRoom) {
      ImGui::Text("Width");
      ImGui::SameLine();
      float width = room_.simple_room().width();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomWidth", &width, 10, 1, "%.0f");
      room_.mutable_simple_room()->set_width(width);

      ImGui::Text("Height");
      ImGui::SameLine();
      float height = room_.simple_room().height();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomHeight", &height, 10, 1, "%.0f");
      room_.mutable_simple_room()->set_height(height);
    }

    if (room_.type_case() == Room::kBarrelRoom) {
      ImGui::Text("Radius");
      ImGui::SameLine();
      float radius = room_.barrel_room().radius();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomRadius", &radius, 5, 1, "%.0f");
      room_.mutable_barrel_room()->set_radius(radius);
    }

    if (room_.type_case() == Room::kCylinderRoom) {
      ImGui::Text("Height");
      ImGui::SameLine();
      float height = room_.cylinder_room().height();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomHeight", &height, 10, 1, "%.0f");

      bool use_width_percent = room_.cylinder_room().width_perimeter_percent() > 0;
      ImGui::Text("Width as percent of perimeter?");
      ImGui::SameLine();
      ImGui::Checkbox("##WidthPercentCheckbox", &use_width_percent);

      ImGui::Text("Width");
      ImGui::SameLine();
      if (use_width_percent) {
        float width_percent = room_.cylinder_room().width_perimeter_percent() * 100;
        if (width_percent <= 0) {
          width_percent = 40;
        }
        ImGui::SetNextItemWidth(char_x_ * 12);
        ImGui::InputFloat("##WidthPercent", &width_percent, 5, 1, "%.1f");
        room_.mutable_cylinder_room()->set_width_perimeter_percent(width_percent / 100.0);
        room_.mutable_cylinder_room()->set_width(0);
      } else {
        float width = room_.cylinder_room().width();
        if (width <= 0) {
          width = 100;
        }
        ImGui::SetNextItemWidth(char_x_ * 12);
        ImGui::InputFloat("##Width", &width, 10, 1, "%.0f");
        room_.mutable_cylinder_room()->set_width(width);
        room_.mutable_cylinder_room()->set_width_perimeter_percent(0);
      }

      room_.mutable_cylinder_room()->set_height(height);
      ImGui::Text("Radius");
      ImGui::SameLine();
      float radius = room_.cylinder_room().radius();
      ImGui::SetNextItemWidth(char_x_ * 12);
      ImGui::InputFloat("##RoomRadius", &radius, 10, 1, "%.0f");
      room_.mutable_cylinder_room()->set_radius(radius);

      ImGui::Text("Draw sides");
      ImGui::SameLine();
      bool has_sides = !room_.cylinder_room().hide_sides();
      ImGui::Checkbox("##DrawSides", &has_sides);
      room_.mutable_cylinder_room()->set_hide_sides(!has_sides);

      if (has_sides) {
        float side_angle = room_.cylinder_room().side_angle_degrees();
        if (side_angle <= 0) {
          side_angle = 20;
        }
        ImGui::Text("Side angle degrees");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(char_x_ * 12);
        ImGui::InputFloat("##SideAngle", &side_angle, 1, 1, "%.0f");
        room_.mutable_cylinder_room()->set_side_angle_degrees(side_angle);
      } else {
        room_.mutable_cylinder_room()->clear_side_angle_degrees();
      }
    }

    ImGui::Text("Camera position");
    ImGui::SameLine();
    VectorEditor("CameraPositionVector", room_.mutable_camera_position(), char_size);

    if (ImGui::TreeNode("Advanced")) {
      bool has_camera_up = room_.has_camera_up();
      ImGui::Text("Set camera up");
      ImGui::SameLine();
      ImGui::Checkbox("##CameraUp", &has_camera_up);
      if (has_camera_up) {
        ImGui::SameLine();
        if (IsZero(room_.camera_up())) {
          room_.mutable_camera_up()->set_z(1);
        }
        VectorEditor("CameraUpVector", room_.mutable_camera_up(), char_size);
      } else {
        room_.clear_camera_up();
      }

      bool has_camera_front = room_.has_camera_front();
      ImGui::Text("Set camera front");
      ImGui::SameLine();
      ImGui::Checkbox("##CameraFront", &has_camera_front);
      if (has_camera_front) {
        ImGui::SameLine();
        if (IsZero(room_.camera_front())) {
          room_.mutable_camera_front()->set_y(1);
        }
        VectorEditor("CameraFrontVector", room_.mutable_camera_front(), char_size);
      } else {
        room_.clear_camera_front();
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

    ImGui::Text("Explicit profile selection order");
    bool use_target_order = t->target_order_size() > 0;
    ImGui::SameLine();
    ImGui::Checkbox("##UseTargetOrder", &use_target_order);
    if (use_target_order) {
      ImGui::Indent();
      if (t->target_order_size() == 0) {
        t->add_target_order(0);
      }
      int remove_at_i = -1;
      for (int i = 0; i < t->target_order_size(); ++i) {
        ImGui::IdGuard lid("TargetOrder", i);
        u32 profile_number = t->target_order(i);
        u32 step = 1;
        ImGui::Text("Profile");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(char_x_ * 8);
        ImGui::InputScalar(
            "##TargetOrderItemInput", ImGuiDataType_U32, &profile_number, &step, nullptr, "%u");
        profile_number = std::min<u32>(profile_number, t->profiles_size() - 1);
        t->set_target_order(i, profile_number);

        ImGui::SameLine();
        if (ImGui::Button("x")) {
          remove_at_i = i;
        }
      }
      if (ImGui::Button("Add##target_order")) {
        t->add_target_order(0);
      }
      if (remove_at_i >= 0) {
        t->mutable_target_order()->erase(t->mutable_target_order()->begin() + remove_at_i);
      }
      ImGui::Unindent();
    } else {
      t->clear_target_order();
    }

    bool allow_percents = t->target_order_size() == 0 && t->profiles_size() > 1;
    for (int i = 0; i < t->profiles_size(); ++i) {
      ImGui::IdGuard lid("Profile", i);
      ImGui::Text("Profile #%d", i);
      ImGui::Indent();
      DrawTargetProfile(t->mutable_profiles(i), allow_percents, char_size);
      ImGui::Unindent();
    }

    if (ImGui::Button("Add profile")) {
      t->add_profiles();
    }

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

  void DrawTargetProfile(TargetProfile* profile, bool allow_percents, const ImVec2& char_size) {
    if (allow_percents) {
      ImGui::Text("Percent chance to use");
      ImGui::SameLine();
      int percent = profile->percent_chance() * 100;
      if (percent <= 0) {
        percent = 100;
      }
      ImGui::SetNextItemWidth(char_x_ * 10);
      ImGui::InputInt("##PercentChance", &percent, 5, 10);
      profile->set_percent_chance(percent / 100.0);
    } else {
      profile->clear_percent_chance();
    }

    ImGui::Text("Radius");
    ImGui::SameLine();
    float target_radius = profile->target_radius();
    if (target_radius <= 0) {
      target_radius = 2;
    }
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##TargetRadiusEntry", &target_radius, 0.1, 0.1, "%.1f");
    profile->set_target_radius(target_radius);

    ImGui::SameLine();
    ImGui::Text("+/-");
    ImGui::SameLine();
    float radius_jitter = profile->target_radius_jitter();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##TargetRadiusJitterEntry", &radius_jitter, 0.1, 0.1, "%.1f");
    if (radius_jitter < 0) {
      radius_jitter = 0;
    }
    profile->set_target_radius_jitter(radius_jitter);

    // TODO: target_hit_radius

    ImGui::Text("Speed");
    ImGui::SameLine();
    float speed = profile->speed();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##SpeedEntry", &speed, 1, 5, "%.1f");
    if (speed > 0) {
      profile->set_speed(speed);
    } else {
      profile->clear_speed();
    }

    ImGui::SameLine();
    ImGui::Text("+/-");
    ImGui::SameLine();
    float speed_jitter = profile->speed_jitter();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##SpeedJitterEntry", &speed_jitter, 1, 5, "%.1f");
    profile->set_speed_jitter(speed_jitter);

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

  void Render() override {
    target_manager_.UpdateRoom(room_);
    CameraParams camera_params(room_);
    Camera camera(camera_params);
    auto look_at = camera.GetLookAt();

    RenderContext ctx;
    Stopwatch stopwatch;
    FrameTimes frame_times;
    if (app_->StartRender(&ctx)) {
      app_->renderer()->DrawScenario(projection_,
                                     room_,
                                     theme_,
                                     target_manager_.GetTargets(),
                                     look_at.transform,
                                     &ctx,
                                     stopwatch,
                                     &frame_times);
      app_->FinishRender(&ctx);
    }
  }

 private:
  Room room_;
  ScenarioDef def_;
  TargetManager target_manager_;
  glm::mat4 projection_;
  Theme theme_;
  float char_x_ = 0;
  ImVec2 char_size_{};
};

}  // namespace

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(Application* app) {
  return std::make_unique<ScenarioEditorScreen>(app);
}

}  // namespace aim

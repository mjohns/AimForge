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
    const ScreenInfo& screen = app_->screen_info();
    ImVec2 char_size = ImGui::CalcTextSize("A");

    if (ImGui::TreeNode("Room")) {
      DrawRoomEditor(char_size);
      ImGui::TreePop();
    }

    {
      ImVec2 sz = ImVec2(char_size.x * 14, 0.0f);
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

  void DrawRoomEditor(const ImVec2& char_size) {
    ImGui::IdGuard cid("RoomEditor");

    ImGuiComboFlags combo_flags = 0;
    if (room_.type_case() == Room::TYPE_NOT_SET) {
      room_ = GetDefaultSimpleRoom();
    }

    ImGui::PushItemWidth(char_size.x * 25);
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
      ImGui::Text("Height");
      ImGui::SameLine();
      float height = room_.simple_room().height();
      ImGui::SetNextItemWidth(char_size.x * 12);
      ImGui::InputFloat("##RoomHeight", &height, 10, 1, "%.0f");
      room_.mutable_simple_room()->set_height(height);

      ImGui::Text("Width");
      ImGui::SameLine();
      float width = room_.simple_room().width();
      ImGui::SetNextItemWidth(char_size.x * 12);
      ImGui::InputFloat("##RoomWidth", &width, 10, 1, "%.0f");
      room_.mutable_simple_room()->set_width(width);
    }

    if (room_.type_case() == Room::kCylinderRoom) {
      ImGui::Text("Height");
      ImGui::SameLine();
      float height = room_.cylinder_room().height();
      ImGui::SetNextItemWidth(char_size.x * 12);
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
        ImGui::SetNextItemWidth(char_size.x * 12);
        ImGui::InputFloat("##WidthPercent", &width_percent, 5, 1, "%.1f");
        room_.mutable_cylinder_room()->set_width_perimeter_percent(width_percent / 100.0);
        room_.mutable_cylinder_room()->set_width(0);
      } else {
        float width = room_.cylinder_room().width();
        if (width <= 0) {
          width = 100;
        }
        ImGui::SetNextItemWidth(char_size.x * 12);
        ImGui::InputFloat("##Width", &width, 10, 1, "%.0f");
        room_.mutable_cylinder_room()->set_width(width);
        room_.mutable_cylinder_room()->set_width_perimeter_percent(0);
      }

      room_.mutable_cylinder_room()->set_height(height);
      ImGui::Text("Radius");
      ImGui::SameLine();
      float radius = room_.cylinder_room().radius();
      ImGui::SetNextItemWidth(char_size.x * 12);
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
        ImGui::SetNextItemWidth(char_size.x * 12);
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

  void Render() override {
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
  TargetManager target_manager_;
  glm::mat4 projection_;
  Theme theme_;
};
}  // namespace

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(Application* app) {
  return std::make_unique<ScenarioEditorScreen>(app);
}

}  // namespace aim

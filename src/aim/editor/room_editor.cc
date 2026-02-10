#include "room_editor.h"

#include <algorithm>
#include <functional>
#include <string>

#include "absl/strings/ascii.h"
#include "aim/common/field.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/util.h"
#include "aim/editor/scenario_editor_common.h"
#include "aim/proto/scenario.pb.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

namespace aim {

void DrawRoomEditorInputs(Room& room) {
  ImGuiComboFlags combo_flags = 0;
  float char_x = ImGui::GetDefaultCharSizeX();

  auto type = room.type_case();
  if (ImGui::SimpleTypeDropdown("RoomTypeDropdown", &type, kRoomTypes, char_x * 15)) {
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
    ImGui::SetNextItemWidth(char_x * 12);
    ImGui::InputFloat("##RoomWidth", &width, 1, 10, "%.0f");
    room.mutable_simple_room()->set_width(width);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Height");
    ImGui::SameLine();
    float height = room.simple_room().height();
    ImGui::SetNextItemWidth(char_x * 12);
    ImGui::InputFloat("##RoomHeight", &height, 1, 10, "%.0f");
    room.mutable_simple_room()->set_height(height);

    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Depth")
                          .set_is_optional()
                          .set_default(400)
                          .set_step(1, 10)
                          .set_width(char_x * 12),
                      PROTO_FLOAT_FIELD(SimpleRoom, room.mutable_simple_room(), depth));
    ImGui::SameLine();
    ImGui::HelpMarker("If set the room will have a fixed depth and the back wall will be drawn");
  }

  if (room.type_case() == Room::kBarrelRoom) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Radius");
    ImGui::SameLine();
    float radius = room.barrel_room().radius();
    ImGui::SetNextItemWidth(char_x * 12);
    ImGui::InputFloat("##RoomRadius", &radius, 1, 5, "%.0f");
    room.mutable_barrel_room()->set_radius(radius);
  }

  if (room.type_case() == Room::kCylinderRoom) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Height");
    ImGui::SameLine();
    float height = room.cylinder_room().height();
    ImGui::SetNextItemWidth(char_x * 12);
    ImGui::InputFloat("##RoomHeight", &height, 1, 10, "%.0f");

    bool use_width_degrees = room.cylinder_room().width_degrees() > 0;
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Width as degrees");
    ImGui::SameLine();
    ImGui::Checkbox("##WidthPercentCheckbox", &use_width_degrees);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Width");
    ImGui::SameLine();
    if (use_width_degrees) {
      ImGui::InputFloat(
          ImGui::InputFloatParams("WidthDegrees").set_min(1).set_default(90).set_step(1, 5),
          PROTO_FLOAT_FIELD(CylinderRoom, room.mutable_cylinder_room(), width_degrees));
      room.mutable_cylinder_room()->clear_width();
    } else {
      float width = FirstGreaterThanZero(room.cylinder_room().width(), 100);
      ImGui::SetNextItemWidth(char_x * 12);
      ImGui::InputFloat("##Width", &width, 1, 10, "%.0f");
      room.mutable_cylinder_room()->set_width(width);
      room.mutable_cylinder_room()->clear_width_degrees();
    }

    room.mutable_cylinder_room()->set_height(height);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Radius");
    ImGui::SameLine();
    float radius = room.cylinder_room().radius();
    ImGui::SetNextItemWidth(char_x * 12);
    ImGui::InputFloat("##RoomRadius", &radius, 1, 10, "%.0f");
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
      ImGui::SetNextItemWidth(char_x * 12);
      ImGui::InputFloat("##SideAngle", &side_angle, 1, 5, "%.0f");
      room.mutable_cylinder_room()->set_side_angle_degrees(side_angle);
      ImGui::Unindent();
    } else {
      room.mutable_cylinder_room()->clear_side_angle_degrees();
    }
  }

  ImGui::SpacedSeparator();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Camera position");
  ImGui::Indent();
  VectorEditor(
      ImGui::InputFloatParams("CameraPositionVector").set_step(1, 10).set_width(char_x * 10),
      room.mutable_camera_position());
  ImGui::Unindent();

  ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Horizontal Fov")
                        .set_is_optional()
                        .set_step(1, 5)
                        .set_min(1)
                        .set_default(103)
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(Room, &room, horizontal_fov));

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
    VectorEditor(ImGui::InputFloatParams("CameraUpVector").set_step(0.1, 1).set_width(char_x * 10),
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
    VectorEditor(
        ImGui::InputFloatParams("CameraFrontVector").set_step(0.1, 1).set_width(char_x * 10),
        room.mutable_camera_front());
    ImGui::Unindent();
  } else {
    room.clear_camera_front();
  }
}

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

}  // namespace aim

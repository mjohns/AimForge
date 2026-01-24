#include "scenario_editor_common.h"

#include <functional>

#include "aim/common/field.h"
#include "aim/common/imgui_ext.h"
#include "imgui.h"

namespace aim {

void SetRegionLengthValue(RegionLength* length, RegionLength::TypeCase type, float value) {
  switch (type) {
    case RegionLength::kValue:
      length->set_value(value);
      return;
    case RegionLength::kDepthPercentValue:
      length->set_depth_percent_value(value / 100.0f);
      return;
    case RegionLength::kXPercentValue:
      length->set_x_percent_value(value / 100.0f);
      return;
    case RegionLength::kYPercentValue:
      length->set_y_percent_value(value / 100.0f);
      return;
    case RegionLength::TYPE_NOT_SET:
      return;
  }
}

float GetRegionLengthValue(RegionLength* length) {
  switch (length->type_case()) {
    case RegionLength::kValue:
      return length->value();
    case RegionLength::kDepthPercentValue:
      return length->depth_percent_value() * 100;
    case RegionLength::kXPercentValue:
      return length->x_percent_value() * 100;
    case RegionLength::kYPercentValue:
      return length->y_percent_value() * 100;
    case RegionLength::TYPE_NOT_SET:
      return 0;
  }
  return 0;
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
  if (def.has_angle_strafe_def()) {
    return def.angle_strafe_def().target_placement_strategy();
  }
  if (def.has_strafe_def()) {
    return def.strafe_def().target_placement_strategy();
  }
  if (def.has_bounce_def()) {
    return def.bounce_def().target_placement_strategy();
  }
  return {};
}

void DrawRegionLengthEditor(const std::string& id,
                            RegionLength::TypeCase default_type,
                            RegionLength* length,
                            float default_value,
                            bool is_point) {
  float char_x = ImGui::GetDefaultCharSizeX();
  float value = GetRegionLengthValue(length);
  RegionLength::TypeCase type = length->type_case();
  if (type == RegionLength::TYPE_NOT_SET) {
    type = default_type;
    value = default_value;
  }
  ImGui::IdGuard cid(id);

  Field<float> field = CreateFloatField(&value);
  auto params = ImGui::InputFloatParams("ValueInput").set_step(1, 5).set_width(char_x * 9);
  if (!is_point) {
    params.set_min(0);
  }
  ImGui::InputFloat(params, field);

  ImGui::SameLine();
  bool type_changed =
      ImGui::SimpleTypeDropdown("TypeDropdown", &type, kRegionLengthTypes, char_x * 8);

  SetRegionLengthValue(length, type, field.get());
}

void DrawRegionLengthPointEditor(const std::string& id,
                                 RegionLength::TypeCase default_type,
                                 RegionLength* length) {
  return DrawRegionLengthEditor(id, default_type, length, 0, /*is_point=*/true);
}

void DrawJitteredRegionLengthEditor(const std::string& id,
                                    RegionLength::TypeCase default_type,
                                    RegionLength* length,
                                    RegionLength* jitter_length,
                                    float default_value) {
  float char_x = ImGui::GetDefaultCharSizeX();
  float value = GetRegionLengthValue(length);
  float jitter_value = GetRegionLengthValue(jitter_length);
  RegionLength::TypeCase type = length->type_case();
  if (type == RegionLength::TYPE_NOT_SET) {
    type = default_type;
    value = default_value;
  }
  ImGui::IdGuard cid(id);

  Field<float> field = CreateFloatField(&value);
  Field<float> jitter_field = CreateFloatField(&jitter_value);

  auto params = ImGui::InputFloatParams("ValueInput").set_step(1, 5).set_width(char_x * 9);

  ImGui::InputFloat(params, field);
  ImGui::SameLine();
  ImGui::Text("+/-");
  ImGui::SameLine();
  params.set_id("JitteredValueInput").set_step(0.5, 2.5);
  ImGui::InputFloat(params, jitter_field);

  ImGui::SameLine();
  bool type_changed =
      ImGui::SimpleTypeDropdown("TypeDropdown", &type, kRegionLengthTypes, char_x * 8);

  SetRegionLengthValue(length, type, field.get());
  SetRegionLengthValue(jitter_length, type, jitter_field.get());
}

void DrawOptionalRegionLengthEditor(const std::string& id,
                                    RegionLength::TypeCase default_type,
                                    PtrField<RegionLength> length,
                                    float default_value) {
  ImGui::IdGuard cid(id);
  bool has_value = length.has();
  ImGui::Checkbox("##UseRegionLength", &has_value);
  if (has_value) {
    ImGui::SameLine();
    DrawRegionLengthEditor("RegionLength", default_type, length.get_mutable(), default_value);
  } else {
    length.clear();
  }
}

void DrawRegionVec2Editor(const std::string& id, RegionVec2* v) {
  ImGui::IdGuard cid(id);
  ImGui::AlignTextToFramePadding();
  ImGui::Text("x");
  ImGui::SameLine();
  DrawRegionLengthPointEditor("X" + id, RegionLength::kXPercentValue, v->mutable_x());
  ImGui::AlignTextToFramePadding();
  ImGui::Text("y");
  ImGui::SameLine();
  DrawRegionLengthPointEditor("Y" + id, RegionLength::kYPercentValue, v->mutable_y());
}

void VectorEditor(ImGui::InputFloatParams params, StoredVec3* v) {
  ImGui::IdGuard cid(params.id);

  ImGui::InputFloat(params.set_label("X").set_id("##XInput"), PROTO_FLOAT_FIELD(StoredVec3, v, x));

  ImGui::InputFloat(params.set_label("Y").set_id("##YInput"), PROTO_FLOAT_FIELD(StoredVec3, v, y));

  ImGui::InputFloat(params.set_label("Z").set_id("##ZInput"), PROTO_FLOAT_FIELD(StoredVec3, v, z));
}

}  // namespace aim

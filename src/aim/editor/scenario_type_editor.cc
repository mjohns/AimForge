#include "scenario_type_editor.h"

#include <format>
#include <functional>
#include <optional>

#include "absl/strings/ascii.h"
#include "aim/common/field.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/common/util.h"
#include "aim/common/wall.h"
#include "aim/core/application.h"
#include "aim/core/scenario_manager.h"
#include "aim/editor/profile_list_editor.h"
#include "aim/editor/scenario_editor_common.h"
#include "aim/scenario/scenario_overrides.h"
#include "aim/ui/search_selector.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace aim {
namespace {

void DrawWallArcEditor(WallArcScenarioDef& d) {
  ImGui::IdGuard cid("WallArcEditor");

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Width");
  ImGui::SameLine();
  DrawRegionLengthEditor("Width", RegionLength::kXPercentValue, d.mutable_width(), 50);
  ImGui::SameLine();
  ImGui::HelpMarker("The arc will be stretched over the specified width");

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Height");
  ImGui::SameLine();
  DrawRegionLengthEditor("Height", RegionLength::kYPercentValue, d.mutable_height(), 50);

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Height range");
  ImGui::SameLine();
  bool use_range = d.has_height_jitter();
  ImGui::Checkbox("##UseRange", &use_range);
  if (use_range) {
    ImGui::SameLine();
    DrawRegionLengthEditor("Height range", RegionLength::kYPercentValue, d.mutable_height_jitter());
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

void DrawSineEditor(SineScenarioDef& d) {
  ImGui::IdGuard cid("SineEditor");

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Height");
  ImGui::SameLine();
  DrawRegionLengthEditor("Height", RegionLength::kYPercentValue, d.mutable_height(), 20);

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Width");
  ImGui::SameLine();
  DrawRegionLengthEditor("Width", RegionLength::kXPercentValue, d.mutable_width(), 20);

  ImGui::InputBool(ImGui::InputBoolParams("GoingRight").set_label("Going left"),
                   PROTO_BOOL_FIELD(SineScenarioDef, &d, going_left));
}

void DrawCircleEditor(CircleScenarioDef& d) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("CircleEditor");

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Radius");
  ImGui::SameLine();
  DrawRegionLengthEditor("Radius", RegionLength::kXPercentValue, d.mutable_radius(), 50);

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Final radius");
  ImGui::SameLine();
  DrawOptionalRegionLengthEditor("FinalRadius",
                                 RegionLength::kXPercentValue,
                                 PROTO_PTR_FIELD(RegionLength, CircleScenarioDef, &d, final_radius),
                                 50);
  ImGui::SameLine();
  ImGui::HelpMarker(
      "The radius will change to this value over the duration of the scenario (or until "
      "direction change)");

  ImGui::InputFloat(ImGui::InputFloatParams("StartDegrees")
                        .set_label("Start degrees")
                        .set_step(5, 30)
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(CircleScenarioDef, &d, start_degrees));
  ImGui::SameLine();
  ImGui::HelpMarker("0 degrees starts at 3 o'clock and rotates counter clockwise.");

  ImGui::InputBool(ImGui::InputBoolParams("Clockwise").set_label("Start clockwise"),
                   PROTO_BOOL_FIELD(CircleScenarioDef, &d, rotate_clockwise));

  ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Switch direction after time")
                        .set_is_optional()
                        .set_step(1, 5)
                        .set_min(5)
                        .set_default(30)
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(CircleScenarioDef, &d, switch_after_seconds));

  ImGui::SpacedSeparator();

  ImGui::InputFloat(ImGui::InputFloatParams("StretchY")
                        .set_label("Stretch Y")
                        .set_step(0.05, 0.1)
                        .set_default(0.8)
                        .set_min(0.1)
                        .set_is_optional()
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(CircleScenarioDef, &d, stretch_y));
  ImGui::InputFloat(ImGui::InputFloatParams("StretchX")
                        .set_label("Stretch X")
                        .set_step(0.05, 0.1)
                        .set_default(0.8)
                        .set_min(0.1)
                        .set_is_optional()
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(CircleScenarioDef, &d, stretch_x));

  ImGui::SpacedSeparator();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Depth");
  ImGui::SameLine();
  DrawRegionLengthEditor("Depth", RegionLength::kDepthPercentValue, d.mutable_depth(), 0);
  ImGui::SameLine();
  ImGui::HelpMarker("Distance away from the wall");
}

void DrawWallWanderProfile(float char_x, WallWanderProfile* p) {
  ImGui::InputJitteredFloat(ImGui::InputFloatParams("TimeBetweenTurns")
                                .set_label("Time between turns")
                                .set_step(0.1, 2)
                                .set_min(0.1)
                                .set_default(2)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(WallWanderProfile, p, turn_time));
  ImGui::SameLine();
  ImGui::HelpMarker("The amount of time to turn in a single direction before switching.");

  ImGui::InputJitteredFloat(ImGui::InputFloatParams("TurnRate")
                                .set_label("Turn rate")
                                .set_step(10, 30)
                                .set_default(300)
                                .set_min(0)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(WallWanderProfile, p, turn_rate));
  ImGui::SameLine();
  ImGui::HelpMarker(
      "The number of degrees to turn per second. The turn rate will accelerate smoothly "
      "between "
      "turns base on turn time.");
}

void DrawWallWanderEditor(WallWanderScenarioDef& d) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("WallWanderEditor");

  if (d.profiles_size() == 0) {
    d.add_profiles();
  }
  ImGui::Text("Wander profiles");
  ImGui::Indent();
  DrawProfileList("WanderProfileList",
                  "Profile",
                  d.mutable_profile_order(),
                  d.mutable_profiles(),
                  std::bind_front(&DrawWallWanderProfile, char_x));
  ImGui::Unindent();

  ImGui::SpacedSeparator();

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

void DrawLinearEditor(LinearScenarioDef& d) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("LinearEditor");

  ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Angle")
                                .set_step(1, 3)
                                .set_min(0)
                                .set_max(90)
                                .set_default(0)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(LinearScenarioDef, &d, angle));

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Initial left/right direction");
  ImGui::SameLine();
  Direction left_right_direction_type = d.has_left_right_initial_direction()
                                            ? d.left_right_initial_direction()
                                            : Direction::DIRECTION_IN;
  ImGui::SimpleTypeDropdown("LeftRightDirectionTypeDropdown",
                            &left_right_direction_type,
                            kLeftRightDirections,
                            char_x * 20);
  d.set_left_right_initial_direction(left_right_direction_type);

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Initial up/down direction");
  ImGui::SameLine();
  Direction up_down_direction_type = d.up_down_initial_direction();
  ImGui::SimpleTypeDropdown(
      "UpDownDirectionTypeDropdown", &up_down_direction_type, kUpDownDirections, char_x * 20);
  d.set_up_down_initial_direction(up_down_direction_type);

  ImGui::SpacedSeparator();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Initial target location");
  ImGui::Indent();
  DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
  ImGui::Unindent();
}

void DrawBarrelEditor(ScenarioDef& def) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("BarrelEditor");
  BarrelScenarioDef& d = *def.mutable_barrel_def();

  if (!def.room().has_barrel_room()) {
    ImGui::Text("Must use barrel room");
    return;
  }

  ImGui::InputFloat(ImGui::InputFloatParams("DirectionRadiusPercent")
                        .set_label("Redirect to percent of center")
                        .set_step(1, 5)
                        .set_min(1)
                        .set_default(40)
                        .set_width(char_x * 10),
                    PROTO_PERCENT_FIELD(BarrelScenarioDef, &d, direction_radius_percent));
  ImGui::SameLine();
  ImGui::HelpMarker(
      "When the target collides with the wall it will be redirected in the direction of a "
      "random "
      "point within the specified portion of the center. The smaller the radius the more it "
      "will "
      "be redirected towards the center of the circle.");

  if (!d.has_target_placement_strategy()) {
    d.mutable_target_placement_strategy()->mutable_min_distance()->set_value(15);
    CircleTargetRegion* region =
        d.mutable_target_placement_strategy()->add_regions()->mutable_circle();
    region->mutable_diameter()->set_x_percent_value(0.92);
    region->mutable_inner_diameter()->set_x_percent_value(0.6);
  }

  ImGui::SpacedSeparator();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Initial target location");
  ImGui::Indent();
  DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
  ImGui::Unindent();
}

void DrawStrafeProfile(float char_x,
                       RegionLength::TypeCase default_region_length_type,
                       StrafeProfile* p) {
  TimeOrDistance type = p->has_time() ? TimeOrDistance::TIME : TimeOrDistance::DISTANCE;
  ImGui::SimpleTypeDropdown("TimeOrDistanceDropdown", &type, kTimeOrDistance, char_x * 9);
  ImGui::SameLine();
  if (type == TimeOrDistance::TIME) {
    ImGui::InputJitteredFloat(
        ImGui::InputFloatParams("Time").set_step(0.1, 0.5).set_min(0.1).set_default(1).set_width(
            char_x * 10),
        PROTO_JITTERED_FIELD(StrafeProfile, p, time));
    p->clear_distance();
    p->clear_distance_jitter();
  } else {
    DrawJitteredRegionLengthEditor("Distance",
                                   default_region_length_type,
                                   p->mutable_distance(),
                                   p->mutable_distance_jitter(),
                                   30);
    p->clear_time();
    p->clear_time_jitter();
  }
  ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Speed multiplier")
                                .set_is_optional()
                                .set_step(0.05, 0.2)
                                .set_min(0)
                                .set_default(1)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(StrafeProfile, p, speed_multiplier));
  ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Acceleration multiplier")
                                .set_is_optional()
                                .set_step(0.05, 0.2)
                                .set_min(0)
                                .set_default(1)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(StrafeProfile, p, acceleration_multiplier));
  ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Center bias")
                        .set_is_optional()
                        .set_step(0.1, 0.5)
                        .set_min(0.1)
                        .set_default(0.1)
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(StrafeProfile, p, center_bias));
  ImGui::SameLine();
  ImGui::HelpMarker(
      "If close to the edge will shorten/lengthen the next strafe to encourage moving towards the "
      "center. 0.10 means lengthen the strafe by 10%");
}

void DrawStrafeEditor(StrafeScenarioDef& d) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("StrafeEditor");
  DrawBoundsEditor("##Bounds", d.mutable_bounds());

  bool has_relative_bounds = d.has_relative_bounds();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Relative bounds");
  ImGui::SameLine();
  ImGui::Checkbox("##UseRelativeBounds", &has_relative_bounds);
  ImGui::SameLine();
  ImGui::HelpMarker("Constrain movement based on the initial target position");
  if (has_relative_bounds) {
    DrawBoundsEditor("##RelativeBounds", d.mutable_relative_bounds());
  } else {
    d.clear_relative_bounds();
  }

  ImGui::SpacedSeparator();

  ImGui::Text("Left/right profiles");
  ImGui::Indent();
  DrawProfileList("LeftRightProfileList",
                  "Profile",
                  d.mutable_left_right_profile_order(),
                  d.mutable_left_right_profiles(),
                  std::bind_front(&DrawStrafeProfile, char_x, RegionLength::kXPercentValue));
  ImGui::Unindent();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Initial left/right direction");
  ImGui::SameLine();
  Direction left_right_direction = d.left_right_initial_direction();
  ImGui::SimpleTypeDropdown(
      "LeftRightDirectionTypeDropdown", &left_right_direction, kLeftRightDirections, char_x * 18);
  d.set_left_right_initial_direction(left_right_direction);

  ImGui::SpacedSeparator();

  ImGui::Text("Up/down profiles");
  ImGui::Indent();
  DrawProfileList("UpDownProfileList",
                  "Profile",
                  d.mutable_up_down_profile_order(),
                  d.mutable_up_down_profiles(),
                  std::bind_front(&DrawStrafeProfile, char_x, RegionLength::kYPercentValue));
  ImGui::Unindent();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Initial up/down direction");
  ImGui::SameLine();
  Direction up_down_direction = d.up_down_initial_direction();
  ImGui::SimpleTypeDropdown(
      "UpDownDirectionTypeDropdown", &up_down_direction, kUpDownDirections, char_x * 18);
  d.set_up_down_initial_direction(up_down_direction);

  if (d.bounds().has_depth()) {
    ImGui::SpacedSeparator();
    ImGui::Text("Forward/back profiles");
    ImGui::Indent();
    DrawProfileList("ForwardBackProfileList",
                    "Profile",
                    d.mutable_forward_back_profile_order(),
                    d.mutable_forward_back_profiles(),
                    std::bind_front(&DrawStrafeProfile, char_x, RegionLength::kDepthPercentValue));
    ImGui::Unindent();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial forward/back direction");
    ImGui::SameLine();
    Direction forward_back_direction = d.forward_back_initial_direction();
    ImGui::SimpleTypeDropdown("ForwardBackDirectionTypeDropdown",
                              &forward_back_direction,
                              kForwardBackDirections,
                              char_x * 18);
    d.set_forward_back_initial_direction(forward_back_direction);
  } else {
    d.clear_forward_back_profiles();
    d.clear_forward_back_profile_order();
    d.clear_forward_back_initial_direction();
  }

  ImGui::SpacedSeparator();

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

void DrawBounceProfile(float char_x, BounceProfile* p) {
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Bounce height");
  ImGui::SameLine();
  DrawJitteredRegionLengthEditor("BounceHeight",
                                 RegionLength::kYPercentValue,
                                 p->mutable_height(),
                                 p->mutable_height_jitter(),
                                 30);

  ImGui::InputJitteredFloat(ImGui::InputFloatParams("Delay")
                                .set_label("Bounce delay")
                                .set_step(0.05, 0.2)
                                .set_zero_is_unset()
                                .set_min(0)
                                .set_default(0)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(BounceProfile, p, delay_seconds));
  ImGui::InputBool(ImGui::InputBoolParams("OnlyDelayOnFloor")
                       .set_label("Only delay on floor"),
                   PROTO_BOOL_FIELD(BounceProfile, p, only_delay_on_floor));

  ImGui::InputFloat(ImGui::InputFloatParams("SpeedMultiplier")
                        .set_label("Speed multiplier")
                        .set_is_optional()
                        .set_step(0.05, 0.2)
                        .set_min(0)
                        .set_default(1)
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(BounceProfile, p, speed_multiplier));
  ImGui::InputFloat(ImGui::InputFloatParams("AccelerationMultiplier")
                        .set_label("Acceleration multiplier")
                        .set_is_optional()
                        .set_step(0.05, 0.2)
                        .set_min(0)
                        .set_default(1)
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(BounceProfile, p, acceleration_multiplier));
}

void DrawBounceEditor(BounceScenarioDef& d) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("BounceEditor");
  BoundsDimensions dimensions;
  dimensions.draw_height = false;
  DrawBoundsEditor("##Bounds", d.mutable_bounds(), dimensions);

  ImGui::SpacedSeparator();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Floor height");
  ImGui::SameLine();
  bool use_floor_height = d.has_floor_height();
  ImGui::SameLine();
  ImGui::Checkbox("##FloorHeight", &use_floor_height);
  if (use_floor_height) {
    ImGui::SameLine();
    DrawRegionLengthEditor(
        "FloorHeight", RegionLength::kYPercentValue, d.mutable_floor_height(), 0);
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
                  std::bind_front(&DrawBounceProfile, char_x));
  ImGui::Unindent();

  ImGui::SpacedSeparator();

  ImGui::Text("Left/right profiles");
  ImGui::Indent();
  DrawProfileList("LeftRightProfileList",
                  "Profile",
                  d.mutable_left_right_profile_order(),
                  d.mutable_left_right_profiles(),
                  std::bind_front(&DrawStrafeProfile, char_x, RegionLength::kXPercentValue));
  ImGui::Unindent();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Initial left/right direction");
  ImGui::SameLine();
  Direction left_right_direction = d.left_right_initial_direction();
  ImGui::SimpleTypeDropdown(
      "LeftRightDirectionTypeDropdown", &left_right_direction, kLeftRightDirections, char_x * 18);
  d.set_left_right_initial_direction(left_right_direction);

  if (d.bounds().has_depth()) {
    ImGui::SpacedSeparator();
    ImGui::Text("Forward/back profiles");
    ImGui::Indent();
    DrawProfileList("ForwardBackProfileList",
                    "Profile",
                    d.mutable_forward_back_profile_order(),
                    d.mutable_forward_back_profiles(),
                    std::bind_front(&DrawStrafeProfile, char_x, RegionLength::kDepthPercentValue));
    ImGui::Unindent();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Initial forward/back direction");
    ImGui::SameLine();
    Direction forward_back_direction = d.forward_back_initial_direction();
    ImGui::SimpleTypeDropdown("ForwardBackDirectionTypeDropdown",
                              &forward_back_direction,
                              kForwardBackDirections,
                              char_x * 18);
    d.set_forward_back_initial_direction(forward_back_direction);
  } else {
    d.clear_forward_back_profiles();
    d.clear_forward_back_profile_order();
    d.clear_forward_back_initial_direction();
  }

  ImGui::SpacedSeparator();

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

void DrawAngleStrafeProfile(float char_x, AngleStrafeProfile* p) {
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Distance");
  ImGui::SameLine();
  DrawJitteredRegionLengthEditor("Distance",
                                 RegionLength::kXPercentValue,
                                 p->mutable_distance(),
                                 p->mutable_distance_jitter(),
                                 30);

  ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Angle")
                                .set_step(1, 3)
                                .set_min(0)
                                .set_max(60)
                                .set_default(0)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(AngleStrafeProfile, p, angle));

  if (p->angle() > 0 || p->angle_jitter() > 0) {
    ImGui::InputFloat(ImGui::InputFloatParams("DirectionChangePercent")
                          .set_label("Direction change chance")
                          .set_step(1, 5)
                          .set_range(0, 100)
                          .set_default(50)
                          .set_width(char_x * 12),
                      PROTO_PERCENT_FIELD(AngleStrafeProfile, p, direction_change_percent));
  } else {
    p->clear_direction_change_percent();
  }

  ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Speed multiplier")
                                .set_is_optional()
                                .set_step(0.05, 0.2)
                                .set_min(0)
                                .set_default(1)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(AngleStrafeProfile, p, speed_multiplier));
  ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Acceleration multiplier")
                                .set_is_optional()
                                .set_step(0.05, 0.2)
                                .set_min(0)
                                .set_default(1)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(AngleStrafeProfile, p, acceleration_multiplier));
}

void DrawAngleStrafeEditor(AngleStrafeScenarioDef& w) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("AngleStrafeEditor");
  DrawBoundsEditor("##Bounds", w.mutable_bounds());

  if (w.profiles_size() == 0) {
    w.add_profiles();
  }

  ImGui::SpacedSeparator();

  ImGui::Text("Strafe profiles");
  ImGui::Indent();
  DrawProfileList("StrafeProfileList",
                  "Profile",
                  w.mutable_profile_order(),
                  w.mutable_profiles(),
                  std::bind_front(&DrawAngleStrafeProfile, char_x));
  ImGui::Unindent();

  ImGui::Spacing();

  ImGui::SpacedSeparator();

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

void DrawCenteringEditor(CenteringScenarioDef& c) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("CenteringEditor");

  const char* kPoints = "Points";
  const char* kAngle = "Angle";

  std::string type = kPoints;
  if (c.has_angle()) {
    type = kAngle;
  }

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Type");
  ImGui::SameLine();
  ImGui::SimpleDropdown("##TypeDrop", &type, {kPoints, kAngle}, char_x * 13);

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
                                  .set_width(char_x * 12),
                              PROTO_JITTERED_FIELD(CenteringScenarioDef, &c, angle));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Specify just the angle of movement and how far to travel. Typically used with Barrel "
        "rooms.");
    DrawRegionLengthEditor("Length", RegionLength::kXPercentValue, c.mutable_angle_length(), 50);
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
      if (ImGui::Button(icons::kCancel)) {
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

void DrawStaticEditor(StaticScenarioDef& d) {
  ImGui::IdGuard cid("StaticEditor");
  DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
}

void DrawWaypointEditor(WaypointScenarioDef& d) {
  ImGui::IdGuard cid("WaypointEditor");
  DrawTargetPlacementStrategyEditor("Placement", d.mutable_target_placement_strategy());
}

void DrawShotTypeEditor(ScenarioDef& def, bool is_single_target_tracking) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("ShotTypeEditor");
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Shot type");
  ImGui::SameLine();

  ShotType& s = *def.mutable_shot_type();
  if (s.type_case() == ShotType::TYPE_NOT_SET) {
    s.set_click_single(true);
  }
  ShotType::TypeCase type = def.shot_type().type_case();

  auto* shot_types = &kShotTypes;
  if (is_single_target_tracking) {
    shot_types = &kSingleTargetTrackingShotTypes;
  }
  if (ImGui::SimpleTypeDropdown("ShotTypeDropdown", &type, *shot_types, char_x * 15)) {
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
    if (type == ShotType::kTrackingProximity) {
      s.set_tracking_proximity(true);
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
                          .set_default(0.05)
                          .set_is_optional()
                          .set_width(char_x * 10),
                      PROTO_FLOAT_FIELD(ShotType, &s, poke_kill_time_seconds));
  }

  if (type == ShotType::kClickMulti) {
    ImGui::InputInt(ImGui::InputIntParams("ClickCount")
                        .set_label("Clicks to kill")
                        .set_step(1, 2)
                        .set_min(2)
                        .set_default(3)
                        .set_width(char_x * 10),
                    PROTO_INT_FIELD(ShotType, def.mutable_shot_type(), health_clicks));
  }

  if (type == ShotType::kClickMulti || type == ShotType::kClickSingle) {
    ImGui::InputFloat(
        ImGui::InputFloatParams::WithLabelAsId("Accuracy penalty multiplier")
            .set_is_optional()
            .set_step(0.01, 0.25)
            .set_min(0)
            .set_default(1)
            .set_width(char_x * 10),
        PROTO_FLOAT_FIELD(ShotType, def.mutable_shot_type(), accuracy_penalty_multiplier));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "0 means no penalty for missed shots. Default (1) is score-sqrt(accuracy%). 0.5 is half "
        "the default penalty.");

    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Click rate")
                          .set_is_optional()
                          .set_step(0.05, 0.2)
                          .set_min(0.05)
                          .set_default(0.5)
                          .set_width(char_x * 10),
                      PROTO_FLOAT_FIELD(ShotType, def.mutable_shot_type(), click_rate_seconds));
    ImGui::SameLine();
    ImGui::HelpMarker("The amount of time in seconds after shooting before you can shoot again");
  }

  if (type == ShotType::kTrackingKill) {
    ImGui::InputFloat(ImGui::InputFloatParams("HealthSeconds")
                          .set_label("Health time")
                          .set_step(0.01, 0.1)
                          .set_min(0.01)
                          .set_default(0.4)
                          .set_width(char_x * 10),
                      PROTO_FLOAT_FIELD(ShotType, &s, health_seconds));
    ImGui::SameLine();
    ImGui::HelpMarker("The amount of time in seconds to kill the target.");

    ImGui::InputFloat(ImGui::InputFloatParams("HealthRegenRate")
                          .set_label("Health regen rate")
                          .set_step(0.1, 0.5)
                          .set_min(0.1)
                          .set_default(1)
                          .set_is_optional()
                          .set_width(char_x * 10),
                      PROTO_FLOAT_FIELD(ShotType, &s, health_regen_rate));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "The rate health is regenerated if you switch off target before killing. 1 means regen "
        "at same rate as health is taken away for hits.");

    ImGui::InputFloat(ImGui::InputFloatParams("RemoveIfBelowHealthSeconds")
                          .set_label("Remove if below remaining health seconds")
                          .set_step(0.01, 0.05)
                          .set_min(0.01)
                          .set_max(5)
                          .set_default(0.10)
                          .set_is_optional()
                          .set_width(char_x * 10),
                      PROTO_FLOAT_FIELD(ShotType, &s, remove_if_below_health_seconds));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "If the target has less than the specified health time remaining and you are not on "
        "target, remove the target and  receive partial score. The kill sound will  be played "
        "early based on this time.");

    ImGui::InputBool(
        ImGui::InputBoolParams("NoPartialKills").set_label("No partial kills"),
        PROTO_BOOL_FIELD(ShotType, def.mutable_shot_type(), no_partial_kills));
  }
}

void DrawReferenceEditor(ScenarioDef& def, Application* app, std::string* error_message_out) {
  // Make sure only the appropriate fields are set on the def.
  ScenarioDef old_def = def;

  def = {};
  def.set_description(old_def.description());
  *def.mutable_overrides() = old_def.overrides();
  *def.mutable_reference_def() = old_def.reference_def();

  ReferenceScenarioDef& r = *def.mutable_reference_def();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Scenario");
  ImGui::SameLine();
  ImGui::HelpMarker("The name of the scenario to reference");
  ImGui::SameLine();

  if (app != nullptr) {
    ScenarioSearchInput(*app, r.mutable_scenario_name());
  } else {
    ImGui::InputText("##ScenarioReference", r.mutable_scenario_name());
  }

  ImGui::SpacedSeparator();

  ImGui::Text("Overrides");
  ImGui::Indent();
  DrawOverridesEditor("ReferenceOverrides", def.mutable_overrides());
  ImGui::Unindent();

  ImGui::Spacing();
  ImGui::Spacing();

  if (app != nullptr) {
    if (ImGui::Button("Bake")) {
      auto parent = app->scenario_manager().GetEvaluatedScenarioDef(r.scenario_name());
      if (parent) {
        auto overrides = def.overrides();
        def = ApplyScenarioOverrides(*parent);
        *def.mutable_overrides() = overrides;
        def = ApplyScenarioOverrides(def);
      } else {
        *error_message_out =
            std::format("Referenced scenario \"{}\" is invalid.", r.scenario_name());
      }
    }
    ImGui::SameLine();
    ImGui::HelpMarker("Expand and remove the reference. Will now be an equivalent normal scenario");
  }
}

void InitializeScenarioType(ScenarioDef& def, ScenarioDef::TypeCase scenario_type) {
  auto target_placement = GetTargetPlacementStrategy(def);
  if (scenario_type == ScenarioDef::kStaticDef) {
    *def.mutable_static_def()->mutable_target_placement_strategy() = target_placement;
  }
  if (scenario_type == ScenarioDef::kWaypointDef) {
    *def.mutable_waypoint_def()->mutable_target_placement_strategy() = target_placement;
  }
  if (scenario_type == ScenarioDef::kCenteringDef) {
    def.mutable_centering_def();
  }
  if (scenario_type == ScenarioDef::kAngleStrafeDef) {
    auto* angle_strafe = def.mutable_angle_strafe_def();
    if (target_placement.regions_size() > 0) {
      *angle_strafe->mutable_target_placement_strategy() = target_placement;
    }
  }
  if (scenario_type == ScenarioDef::kStrafeDef) {
    auto* strafe = def.mutable_strafe_def();
    if (target_placement.regions_size() > 0) {
      *strafe->mutable_target_placement_strategy() = target_placement;
    }
  }
  if (scenario_type == ScenarioDef::kBounceDef) {
    auto* d = def.mutable_bounce_def();
    if (target_placement.regions_size() > 0) {
      *d->mutable_target_placement_strategy() = target_placement;
    }
  }
  if (scenario_type == ScenarioDef::kLinearDef) {
    *def.mutable_linear_def()->mutable_target_placement_strategy() = target_placement;
  }
  if (scenario_type == ScenarioDef::kBarrelDef) {
    def.mutable_barrel_def();
  }
  if (scenario_type == ScenarioDef::kWallArcDef) {
    def.mutable_wall_arc_def();
  }
  if (scenario_type == ScenarioDef::kReferenceDef) {
    def.mutable_reference_def();
  }
}

}  // namespace

void DrawScenarioTypeEditor(ScenarioDef& def, Application* app, std::string* error_message_out) {
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::IdGuard cid("ScenarioTypeEditor");
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Scenario type");
  ImGui::SameLine();

  if (def.type_case() == ScenarioDef::TYPE_NOT_SET) {
    def.mutable_static_def();
  }

  auto scenario_type = def.type_case();
  bool is_new_type = ImGui::SimpleTypeDropdown(
      "ScenarioTypeDropdown", &scenario_type, kScenarioTypes, char_x * 15);
  InitializeScenarioType(def, scenario_type);

  bool is_single_target_tracking = VectorContains(kSingleTargetTrackingTypes, scenario_type);
  if (is_single_target_tracking) {
    if (is_new_type) {
      def.mutable_shot_type()->set_tracking_invincible(true);
      def.clear_target_def();
    }
  }

  ImGui::SpacedSeparator();

  if (scenario_type != ScenarioDef::kReferenceDef) {
    DrawShotTypeEditor(def, is_single_target_tracking);
    ImGui::SpacedSeparator();
  }

  if (scenario_type == ScenarioDef::kReferenceDef) {
    DrawReferenceEditor(def, app, error_message_out);
  }
  if (scenario_type == ScenarioDef::kStaticDef) {
    DrawStaticEditor(*def.mutable_static_def());
  }
  if (scenario_type == ScenarioDef::kWaypointDef) {
    DrawWaypointEditor(*def.mutable_waypoint_def());
  }
  if (scenario_type == ScenarioDef::kCenteringDef) {
    DrawCenteringEditor(*def.mutable_centering_def());
  }
  if (scenario_type == ScenarioDef::kAngleStrafeDef) {
    DrawAngleStrafeEditor(*def.mutable_angle_strafe_def());
  }
  if (scenario_type == ScenarioDef::kStrafeDef) {
    DrawStrafeEditor(*def.mutable_strafe_def());
  }
  if (scenario_type == ScenarioDef::kBounceDef) {
    DrawBounceEditor(*def.mutable_bounce_def());
  }
  if (scenario_type == ScenarioDef::kLinearDef) {
    DrawLinearEditor(*def.mutable_linear_def());
  }
  if (scenario_type == ScenarioDef::kBarrelDef) {
    DrawBarrelEditor(def);
  }
  if (scenario_type == ScenarioDef::kWallArcDef) {
    DrawWallArcEditor(*def.mutable_wall_arc_def());
  }
  if (scenario_type == ScenarioDef::kWallWanderDef) {
    DrawWallWanderEditor(*def.mutable_wall_wander_def());
  }
  if (scenario_type == ScenarioDef::kCircleDef) {
    DrawCircleEditor(*def.mutable_circle_def());
  }
  if (scenario_type == ScenarioDef::kSineDef) {
    DrawSineEditor(*def.mutable_sine_def());
  }
}

}  // namespace aim

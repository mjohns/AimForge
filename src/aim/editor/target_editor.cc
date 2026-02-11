#include "target_editor.h"

#include <algorithm>
#include <functional>
#include <string>

#include "absl/strings/ascii.h"
#include "aim/common/field.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/util.h"
#include "aim/editor/profile_list_editor.h"
#include "aim/editor/scenario_editor_common.h"
#include "aim/proto/scenario.pb.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

namespace aim {
namespace {

void DrawTargetProfile(float char_x, ScenarioDef& def, TargetProfile* profile) {
  ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Radius")
                                .set_step(0.05, 0.5)
                                .set_min(0.01)
                                .set_default(2)
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(TargetProfile, profile, target_radius));

  ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Speed")
                                .set_step(1, 10)
                                .set_min(0)
                                .set_zero_is_unset()
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(TargetProfile, profile, speed));

  ImGui::InputJitteredFloat(ImGui::InputFloatParams::WithLabelAsId("Acceleration")
                                .set_step(5, 50)
                                .set_min(1)
                                .set_default(200)
                                .set_is_optional()
                                .set_width(char_x * 10),
                            PROTO_JITTERED_FIELD(TargetProfile, profile, acceleration));

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
    ImGui::SetNextItemWidth(char_x * 12);
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
    ImGui::SetNextItemWidth(char_x * 10);
    ImGui::InputFloat("##GrowthTime", &growth_time, 0.1, 0.5, "%.1f");
    profile->set_target_radius_growth_time_seconds(std::max(growth_time, 0.1f));

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Final radius");
    ImGui::SameLine();
    float final_radius =
        FirstGreaterThanZero(profile->target_radius_growth_size(), profile->target_radius() * 3);
    ImGui::SetNextItemWidth(char_x * 10);
    ImGui::InputFloat("##FinalGrowthRadius", &final_radius, 0.1, 0.5, "%.1f");
    profile->set_target_radius_growth_size(std::max(final_radius, 0.1f));

    ImGui::InputFloat(
        ImGui::InputFloatParams::WithLabelAsId("Time at final size")
            .set_step(0.1, 0.5)
            .set_min(0)
            .set_default(0)
            .set_is_optional()
            .set_width(char_x * 10),
        PROTO_FLOAT_FIELD(TargetProfile, profile, target_radius_growth_final_size_time_seconds));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Time target will stay at final size before being removed. Defaults to 0. i.e. remove "
        "immediately upon reaching final size. This time is in addition to the pulse time.");
    ImGui::Unindent();
  } else {
    profile->clear_target_radius_growth_time_seconds();
    profile->clear_target_radius_growth_final_size_time_seconds();
    profile->clear_target_radius_growth_size();
  }

  if (def.shot_type().type_case() == ShotType::kClickMulti ||
      def.shot_type().type_case() == ShotType::kTrackingKill) {
    ImGui::InputFloat(ImGui::InputFloatParams("TargetRadiusAtill")
                          .set_label("Target radius at kill")
                          .set_step(0.1, 0.5)
                          .set_min(0.1)
                          .set_default(profile->target_radius())
                          .set_is_optional()
                          .set_width(char_x * 10),
                      PROTO_FLOAT_FIELD(TargetProfile, profile, target_radius_at_kill));
    ImGui::SameLine();
    ImGui::HelpMarker(
        "The radius of the target will change to the specified value incrementally based on "
        "how much health remains");
  } else {
    profile->clear_target_radius_at_kill();
  }
}

}  // namespace

void DrawTargetEditor(ScenarioDef& def) {
  ImGui::IdGuard cid("TargetEditor");
  TargetDef* t = def.mutable_target_def();

  float char_x = ImGui::GetDefaultCharSizeX();

  bool is_single_target_tracking = VectorContains(kSingleTargetTrackingTypes, def.type_case());
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

    DrawTargetProfile(char_x, def, t->mutable_profiles(0));
    return;
  }

  int num_targets = t->num_targets();
  if (num_targets <= 0) {
    num_targets = 1;
  }
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Number of targets");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(char_x * 8);
  ImGui::InputInt("##NumberEntry", &num_targets, 1, 1);
  t->set_num_targets(num_targets);

  if (t->profiles_size() == 0) {
    t->add_profiles();
  }

  ImGui::SpacedSeparator();

  ImGui::Text("Target profiles");
  ImGui::Indent();
  DrawProfileList("ProfileList",
                  "Profile",
                  PROTO_PTR_FIELD(ProfileListInfo, TargetDef, t, profiles_info),
                  t->mutable_profiles(),
                  std::bind_front(&DrawTargetProfile, char_x, def));
  ImGui::Unindent();

  ImGui::SpacedSeparator();

  ImGui::AlignTextToFramePadding();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Newest target is ghost");
  ImGui::SameLine();
  bool is_ghost = t->newest_target_is_ghost();
  ImGui::Checkbox("##IsGhost", &is_ghost);
  t->set_newest_target_is_ghost(is_ghost);
  ImGui::SameLine();
  ImGui::HelpMarker("Ghost targets are unkillable and drawn in a different color.");

  ImGui::SpacedSeparator();

  ImGui::InputFloat(ImGui::InputFloatParams("NewTargetDelaySeconds")
                        .set_label("New target delay")
                        .set_is_optional()
                        .set_zero_is_unset()
                        .set_step(0.05, 0.25)
                        .set_min(0.01)
                        .set_default(0.2)
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(TargetDef, t, new_target_delay_seconds));

  ImGui::InputFloat(ImGui::InputFloatParams("RemoveTargetAfterSeconds")
                        .set_label("Remove after time")
                        .set_is_optional()
                        .set_zero_is_unset()
                        .set_step(0.05, 0.25)
                        .set_min(0.01)
                        .set_default(0.2)
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(TargetDef, t, remove_target_after_seconds));

  ImGui::InputFloat(ImGui::InputFloatParams("StaggerInitialTargetsSeconds")
                        .set_label("Initial stagger time")
                        .set_is_optional()
                        .set_zero_is_unset()
                        .set_step(0.05, 0.25)
                        .set_min(0.01)
                        .set_default(0.2)
                        .set_width(char_x * 10),
                    PROTO_FLOAT_FIELD(TargetDef, t, stagger_initial_targets_seconds));
  ImGui::SameLine();
  ImGui::HelpMarker(
      "Time in seconds between each target being added at the start of the scenario.");
}

}  // namespace aim

#include "scenario_overrides.h"

#include "aim/common/util.h"

namespace aim {
namespace {

void MultiplyTargetRadiusValues(TargetProfile* profile, float mult) {
  profile->set_target_radius(profile->target_radius() * mult);
  if (profile->has_target_radius_at_kill()) {
    profile->set_target_radius_at_kill(profile->target_radius_at_kill() * mult);
  }
  if (profile->has_target_radius_growth_size()) {
    profile->set_target_radius_growth_size(profile->target_radius_growth_size() * mult);
  }
}

void MultiplyRegionLength(RegionLength* l, float mult) {
  if (l->has_depth_percent_value()) {
    l->set_depth_percent_value(l->depth_percent_value() * mult);
  }
  if (l->has_x_percent_value()) {
    l->set_x_percent_value(l->x_percent_value() * mult);
  }
  if (l->has_y_percent_value()) {
    l->set_y_percent_value(l->y_percent_value() * mult);
  }
  if (l->has_value()) {
    l->set_value(l->value() * mult);
  }
}

template <typename T>
void ApplyDistanceMultiplier(T& profile_list, float mult) {
  for (auto& profile : profile_list) {
    if (profile.has_distance()) {
      MultiplyRegionLength(profile.mutable_distance(), mult);
    }
    if (profile.has_distance_jitter()) {
      MultiplyRegionLength(profile.mutable_distance_jitter(), mult);
    }
  }
}

template <typename T>
void ApplyTimeScaleMultiplier(T& profile_list, float mult) {
  for (auto& profile : profile_list) {
    if (profile.has_time()) {
      profile.set_time(profile.time() * mult);
    }
    if (profile.has_time_jitter()) {
      profile.set_time_jitter(profile.time_jitter() * mult);
    }
  }
}

ScenarioDef ApplyLeveledOverrides(const ScenarioDef& original,
                                  const ScenarioOverrides& overrides,
                                  std::optional<float> levels) {
  ScenarioDef result = original;

  auto get_multiplier = [=](float multiplier) {
    return levels.has_value() ? std::pow(multiplier, *levels) : multiplier;
  };

  if (overrides.has_target_radius_multiplier()) {
    float mult = get_multiplier(overrides.target_radius_multiplier());
    for (auto& profile : *result.mutable_target_def()->mutable_profiles()) {
      MultiplyTargetRadiusValues(&profile, mult);
    }
  }
  if (overrides.has_growth_time_multiplier()) {
    float mult = get_multiplier(overrides.growth_time_multiplier());
    for (auto& profile : *result.mutable_target_def()->mutable_profiles()) {
      if (profile.has_target_radius_growth_time_seconds()) {
        profile.set_target_radius_growth_time_seconds(profile.target_radius_growth_time_seconds() *
                                                      mult);
      }
      if (profile.has_target_radius_growth_final_size_time_seconds()) {
        profile.set_target_radius_growth_final_size_time_seconds(
            profile.target_radius_growth_final_size_time_seconds() * mult);
      }
    }
  }
  if (overrides.has_remove_target_after_seconds_multiplier()) {
    float mult = get_multiplier(overrides.remove_target_after_seconds_multiplier());
    if (result.target_def().has_remove_target_after_seconds()) {
      result.mutable_target_def()->set_remove_target_after_seconds(
          result.target_def().remove_target_after_seconds() * mult);
    }
  }
  if (overrides.has_speed_multiplier()) {
    float mult = get_multiplier(overrides.speed_multiplier());
    for (auto& profile : *result.mutable_target_def()->mutable_profiles()) {
      profile.set_speed(profile.speed() * mult);
    }
  }
  if (overrides.has_acceleration_multiplier()) {
    float mult = get_multiplier(overrides.acceleration_multiplier());
    for (auto& profile : *result.mutable_target_def()->mutable_profiles()) {
      profile.set_acceleration(profile.acceleration() * mult);
    }
  }
  if (overrides.has_distance_multiplier()) {
    float mult = get_multiplier(overrides.distance_multiplier());
    if (original.has_strafe_def()) {
      ApplyDistanceMultiplier(*result.mutable_strafe_def()->mutable_left_right_profiles(), mult);
      ApplyDistanceMultiplier(*result.mutable_strafe_def()->mutable_up_down_profiles(), mult);
      ApplyDistanceMultiplier(*result.mutable_strafe_def()->mutable_forward_back_profiles(), mult);
    }
    if (original.has_bounce_def()) {
      ApplyDistanceMultiplier(*result.mutable_bounce_def()->mutable_left_right_profiles(), mult);
      ApplyDistanceMultiplier(*result.mutable_bounce_def()->mutable_forward_back_profiles(), mult);
    }
    if (original.has_angle_strafe_def()) {
      ApplyDistanceMultiplier(*result.mutable_angle_strafe_def()->mutable_profiles(), mult);
    }
  }
  if (overrides.has_time_scale_multiplier()) {
    float mult = get_multiplier(overrides.time_scale_multiplier());
    if (original.has_strafe_def()) {
      ApplyTimeScaleMultiplier(*result.mutable_strafe_def()->mutable_left_right_profiles(), mult);
      ApplyTimeScaleMultiplier(*result.mutable_strafe_def()->mutable_up_down_profiles(), mult);
      ApplyTimeScaleMultiplier(*result.mutable_strafe_def()->mutable_forward_back_profiles(), mult);
    }
    if (original.has_bounce_def()) {
      ApplyTimeScaleMultiplier(*result.mutable_bounce_def()->mutable_left_right_profiles(), mult);
      ApplyTimeScaleMultiplier(*result.mutable_bounce_def()->mutable_forward_back_profiles(), mult);
    }
    if (original.has_wall_wander_def()) {
      for (auto& profile : *result.mutable_wall_wander_def()->mutable_profiles()) {
        if (profile.has_turn_time()) {
          profile.set_turn_time(profile.turn_time() * mult);
        }
        if (profile.has_turn_time_jitter()) {
          profile.set_turn_time_jitter(profile.turn_time_jitter() * mult);
        }
      }
    }
  }
  return result;
}

}  // namespace

ScenarioDef ApplyScenarioOverrides(const ScenarioDef& original) {
  if (!original.has_overrides()) {
    return original;
  }
  ScenarioDef result = ApplyLeveledOverrides(original, original.overrides(), {});
  result.clear_overrides();

  return result;
}

ScenarioDef ApplyScenarioLevelOverrides(const ScenarioDef& original_def, float levels) {
  ScenarioDef result = ApplyScenarioOverrides(original_def);
  if (levels == 0) {
    return result;
  }
  return ApplyLeveledOverrides(result, result.level_overrides(), levels);
}

void ApplyReferenceFieldOverrides(const ScenarioDef& ref, ScenarioDef* def) {
  if (ref.has_overrides()) {
    *def->mutable_overrides() = ref.overrides();
  }
  if (ref.has_level_overrides()) {
    *def->mutable_level_overrides() = ref.level_overrides();
  }
  if (ref.has_score_targets()) {
    *def->mutable_score_targets() = ref.score_targets();
  }
  if (ref.reference_def().has_shot_type()) {
    *def->mutable_shot_type() = ref.reference_def().shot_type();
  }
  if (ref.reference_def().has_duration_seconds()) {
    def->set_duration_seconds(ref.reference_def().duration_seconds());
  }
  if (ref.reference_def().has_num_targets()) {
    def->mutable_target_def()->set_num_targets(ref.reference_def().num_targets());
  }
  if (ref.reference_def().has_room()) {
    *def->mutable_room() = ref.reference_def().room();
  }
  if (ref.reference_def().horizontal_fov() > 0) {
    // Important to keep this after the room override has maybe been set.
    def->mutable_room()->set_horizontal_fov(ref.reference_def().horizontal_fov());
  }
  if (!ref.reference_def().description().empty()) {
    def->set_description(ref.reference_def().description());
  }
  float explicit_radius = ref.reference_def().explicit_target_radius();
  if (explicit_radius > 0 && def->target_def().profiles_size() > 0) {
    float first_radius = def->target_def().profiles(0).target_radius();
    if (first_radius > 0) {
      float mult = explicit_radius / first_radius;
      for (TargetProfile& profile : *def->mutable_target_def()->mutable_profiles()) {
        MultiplyTargetRadiusValues(&profile, mult);
      }
    }
  }
}

}  // namespace aim

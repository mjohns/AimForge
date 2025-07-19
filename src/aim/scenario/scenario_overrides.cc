#include "scenario_overrides.h"

#include "aim/common/util.h"

namespace aim {
namespace {

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

}  // namespace

ScenarioDef ApplyScenarioOverrides(const ScenarioDef& original) {
  if (!original.has_overrides()) {
    return original;
  }
  ScenarioDef result = original;
  result.clear_overrides();

  const ScenarioOverrides& overrides = original.overrides();

  if (overrides.has_duration_seconds()) {
    result.set_duration_seconds(overrides.duration_seconds());
  }
  if (overrides.has_num_targets()) {
    result.mutable_target_def()->set_num_targets(overrides.num_targets());
  }
  if (overrides.has_target_radius_multiplier()) {
    float mult = overrides.target_radius_multiplier();
    for (auto& profile : *result.mutable_target_def()->mutable_profiles()) {
      profile.set_target_radius(profile.target_radius() * mult);
      if (profile.has_target_radius_at_kill()) {
        profile.set_target_radius_at_kill(profile.target_radius_at_kill() * mult);
      }
      if (profile.has_target_radius_growth_size()) {
        profile.set_target_radius_growth_size(profile.target_radius_growth_size() * mult);
      }
    }
  }
  if (overrides.has_speed_multiplier()) {
    float mult = overrides.speed_multiplier();
    for (auto& profile : *result.mutable_target_def()->mutable_profiles()) {
      profile.set_speed(profile.speed() * mult);
    }
  }
  if (overrides.has_distance_multiplier()) {
    float mult = overrides.distance_multiplier();
    for (auto& profile : *result.mutable_wall_strafe_def()->mutable_profiles()) {
      MultiplyRegionLength(profile.mutable_distance(), mult);
    }
  }
  if (overrides.has_acceleration_multiplier()) {
    float mult = overrides.acceleration_multiplier();
    if (original.has_wall_strafe_def()) {
      float accel = original.wall_strafe_def().acceleration();
      if (accel > 0) {
        result.mutable_wall_strafe_def()->set_acceleration(accel * mult);
      }
    }
    if (original.has_timed_direction_def()) {
      float accel = original.timed_direction_def().acceleration();
      if (accel > 0) {
        result.mutable_timed_direction_def()->set_acceleration(accel * mult);
      }
    }
  }
  if (overrides.has_time_scale_multiplier()) {
    float mult = overrides.time_scale_multiplier();
    if (original.has_timed_direction_def()) {
      float time_scale =
          FirstGreaterThanZero(original.timed_direction_def().time_scale_multiplier(), 1.0);
      result.mutable_timed_direction_def()->set_time_scale_multiplier(time_scale * mult);
    }
    if (original.has_bounce_def()) {
      float time_scale = FirstGreaterThanZero(original.bounce_def().time_scale_multiplier(), 1.0);
      result.mutable_bounce_def()->set_time_scale_multiplier(time_scale * mult);
    }
    if (original.has_wall_wander_def()) {
      float time_scale =
          FirstGreaterThanZero(original.wall_wander_def().time_scale_multiplier(), 1.0);
      result.mutable_wall_wander_def()->set_time_scale_multiplier(time_scale * mult);
    }
  }
  return result;
}

}  // namespace aim
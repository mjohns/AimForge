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
    if (original.has_angle_strafe_def()) {
      for (auto& profile : *result.mutable_angle_strafe_def()->mutable_profiles()) {
        MultiplyRegionLength(profile.mutable_distance(), mult);
      }
    }
    if (original.has_strafe_def()) {
      float distance = FirstGreaterThanZero(original.strafe_def().distance_multiplier(), 1.0);
      result.mutable_strafe_def()->set_distance_multiplier(distance * mult);
    }
    if (original.has_bounce_def()) {
      float distance = FirstGreaterThanZero(original.bounce_def().distance_multiplier(), 1.0);
      result.mutable_bounce_def()->set_distance_multiplier(distance * mult);
    }
  }
  if (overrides.has_time_scale_multiplier()) {
    float mult = get_multiplier(overrides.time_scale_multiplier());
    if (original.has_strafe_def()) {
      float time_scale = FirstGreaterThanZero(original.strafe_def().time_scale_multiplier(), 1.0);
      result.mutable_strafe_def()->set_time_scale_multiplier(time_scale * mult);
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

}  // namespace aim
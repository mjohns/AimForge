#include "scenario_util.h"

namespace aim {

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
  if (def.has_barrel_def()) {
    return def.barrel_def().target_placement_strategy();
  }
  return {};
}

std::optional<PtrField<TargetPlacementStrategy>> GetTargetPlacementStrategyField(ScenarioDef* def) {
  if (def->has_static_def()) {
    return PROTO_PTR_FIELD(TargetPlacementStrategy,
                           StaticScenarioDef,
                           def->mutable_static_def(),
                           target_placement_strategy);
  }
  if (def->has_waypoint_def()) {
    return PROTO_PTR_FIELD(TargetPlacementStrategy,
                           WaypointScenarioDef,
                           def->mutable_waypoint_def(),
                           target_placement_strategy);
  }
  if (def->has_linear_def()) {
    return PROTO_PTR_FIELD(TargetPlacementStrategy,
                           LinearScenarioDef,
                           def->mutable_linear_def(),
                           target_placement_strategy);
  }
  if (def->has_wall_wander_def()) {
    return PROTO_PTR_FIELD(TargetPlacementStrategy,
                           WallWanderScenarioDef,
                           def->mutable_wall_wander_def(),
                           target_placement_strategy);
  }
  if (def->has_angle_strafe_def()) {
    return PROTO_PTR_FIELD(TargetPlacementStrategy,
                           AngleStrafeScenarioDef,
                           def->mutable_angle_strafe_def(),
                           target_placement_strategy);
  }
  if (def->has_strafe_def()) {
    return PROTO_PTR_FIELD(TargetPlacementStrategy,
                           StrafeScenarioDef,
                           def->mutable_strafe_def(),
                           target_placement_strategy);
  }
  if (def->has_bounce_def()) {
    return PROTO_PTR_FIELD(TargetPlacementStrategy,
                           BounceScenarioDef,
                           def->mutable_bounce_def(),
                           target_placement_strategy);
  }
  if (def->has_barrel_def()) {
    return PROTO_PTR_FIELD(TargetPlacementStrategy,
                           BarrelScenarioDef,
                           def->mutable_barrel_def(),
                           target_placement_strategy);
  }
  return {};
}

}  // namespace aim
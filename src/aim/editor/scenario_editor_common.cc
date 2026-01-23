#include "scenario_editor_common.h"

#include <format>
#include <functional>
#include <optional>

#include "absl/strings/ascii.h"
#include "aim/common/field.h"
#include "aim/common/files.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/name_util.h"
#include "aim/common/resource_name.h"
#include "aim/common/search.h"
#include "aim/common/util.h"
#include "aim/common/wall.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/camera.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"
#include "aim/graphics/renderer.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/scenario_factory.h"
#include "aim/scenario/scenario_overrides.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

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
  return {};
}

}  // namespace aim

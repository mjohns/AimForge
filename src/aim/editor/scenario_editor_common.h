#pragma once

#include <memory>
#include <optional>
#include <string>

#include "aim/core/application.h"
#include "aim/core/scenario_manager.h"
#include "aim/ui/ui_screen.h"

namespace aim {

struct BoundsDimensions {
  bool draw_width = true;
  bool draw_height = true;
  bool draw_depth = true;
};

TargetPlacementStrategy GetTargetPlacementStrategy(const ScenarioDef& def);

}  // namespace aim

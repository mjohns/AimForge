#pragma once

#include <glm/vec2.hpp>
#include <memory>

#include "aim/common/simple_types.h"
#include "aim/common/wall.h"
#include "aim/core/application.h"
#include "aim/core/target.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

class WallTargetPlacer {
 public:
  virtual ~WallTargetPlacer() {}

  // z represents the distance away from the wall for the target. 0 means on the wall.
  virtual glm::vec3 GetNextPosition() = 0;
  virtual glm::vec3 GetNextPosition(int counter) = 0;
};

std::unique_ptr<WallTargetPlacer> CreateWallTargetPlacer(const ScenarioDef& def,
                                                         TargetManager* target_manager,
                                                         Application* app);

std::unique_ptr<WallTargetPlacer> CreateWallTargetPlacer(const Wall& wall,
                                                         const TargetPlacementStrategy& strategy,
                                                         TargetManager* target_manager,
                                                         Application* app);

}  // namespace aim

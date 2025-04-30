#include <SDL3/SDL.h>
#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <random>

#include "aim/common/geometry.h"
#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/proto/common.pb.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/settings.pb.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"

namespace aim {
namespace {

class WallMovementScenario : public BaseScenario {
 public:
  explicit WallMovementScenario(const ScenarioDef& def, Application* app)
      : BaseScenario(def, app), wall_(GetWallForRoom(def.room())) {
    auto m = def.wall_movement_def();
  }

 protected:
  void FillInNewTarget(Target* target) override {
  }

  void UpdateTargetPositions() override {}

 private:
  Wall wall_;
};

}  // namespace

std::unique_ptr<Scenario> CreateWallMovementScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<WallMovementScenario>(def, app);
}

}  // namespace aim

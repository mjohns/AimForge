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

class StaticScenario : public BaseScenario {
 public:
  explicit StaticScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app) {
    wall_target_placer_ = CreateWallTargetPlacer(params.def, &target_manager_, &app_);
  }

 protected:
  void FillInNewTarget(Target* target) override {
    glm::vec3 wall_pos = wall_target_placer_->GetNextPosition();
    target->SetWallPosition(wall_pos, def_.room());
  }

  bool ShouldRecordReplay() override {
    return true;
  }

 private:
  std::unique_ptr<WallTargetPlacer> wall_target_placer_;
};

}  // namespace

std::unique_ptr<Scenario> CreateStaticScenario(const CreateScenarioParams& params,
                                               Application* app) {
  return std::make_unique<StaticScenario>(params, app);
}

}  // namespace aim

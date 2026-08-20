#include <memory>

#include "aim/core/application.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"
#include "glm/vec3.hpp"  // IWYU pragma: keep

namespace aim {
namespace {

class StaticScenario : public BaseScenario {
 public:
  explicit StaticScenario(const CreateScenarioParams& params, Application* app)
      : BaseScenario(params, app) {
    wall_target_placer_ =
        CreateWallTargetPlacer(Wall::ForRoom(params.def.room()),
                               params.def.static_def().target_placement_strategy(),
                               &target_manager_,
                               &app_);
  }

 protected:
  void FillInNewTarget(Target* target) override {
    glm::vec3 wall_pos = wall_target_placer_->GetNextPosition();
    target->SetWallPosition(wall_pos, def_.room());
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

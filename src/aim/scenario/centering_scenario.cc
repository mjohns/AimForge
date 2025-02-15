#include <SDL3/SDL.h>
#include <imgui.h>

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <random>

#include "aim/common/time_util.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/proto/common.pb.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/settings.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {
namespace {

class CenteringScenario : public Scenario {
 public:
  explicit CenteringScenario(const ScenarioDef& def, Application* app) : Scenario(def, app) {}

 protected:
  virtual void OnBeforeEventHandling() {
    if (timer_.IsNewReplayFrame()) {
      // Store the look at vector before the mouse updates for the old frame.
      replay_->add_pitch_yaws(camera_.GetPitch());
      replay_->add_pitch_yaws(camera_.GetYaw());
    }
  }

  void Render() override {
    app_->GetRenderer()->DrawScenario(
        def_.room(), theme_, target_manager_.GetTargets(), look_at_.transform);
  }

  void Initialize() override {
    *replay_->mutable_room() = def_.room();
    replay_->set_replay_fps(timer_.GetReplayFps());
    replay_->set_is_poke_ball(def_.static_def().is_poke_ball());

    // Add the target.
    // Target target = target_manager_.AddTarget(GetNewTarget(def_, &target_manager_, app_));
    // AddNewTargetEvent(target, 0);
  }

  void UpdateState(UpdateStateData* data) override {}

  TargetManager target_manager_;
};

}  // namespace

std::unique_ptr<Scenario> CreateCenteringScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<CenteringScenario>(def, app);
}

}  // namespace aim

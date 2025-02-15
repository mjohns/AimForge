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

constexpr const float kStartMovingDelaySeconds = 1;

class CenteringScenario : public Scenario {
 public:
  explicit CenteringScenario(const ScenarioDef& def, Application* app)
      : Scenario(def, app),
        start_(ToVec3(def.centering_def().start_position())),
        end_(ToVec3(def.centering_def().end_position())) {
    start_to_end_ = glm::normalize(end_ - start_);
    distance_ = glm::length(start_ - end_);
    float time_seconds = def.centering_def().start_to_end_time_seconds();
    if (time_seconds <= 0) {
      time_seconds = 2;
    }
    distance_per_second_ = distance_ / time_seconds;
  }

 protected:
  void Initialize() override {
    Target target;
    target.radius = def_.centering_def().target_width();
    target.position = ToVec3(def_.centering_def().start_position());

    target = target_manager_.AddTarget(target);
    target_ = target_manager_.GetMutableTarget(target.id);

    // AddNewTargetEvent(target, 0);
  }

  void UpdateState(UpdateStateData* data) override {
    if (timer_.GetElapsedSeconds() < kStartMovingDelaySeconds) {
      return;
    }

    auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
    if (data->is_click_held) {
      shot_stopwatch_.Start();
      if (maybe_hit_target_id.has_value()) {
        hit_stopwatch_.Start();
      } else {
        hit_stopwatch_.Stop();
      }
    } else {
      shot_stopwatch_.Stop();
      hit_stopwatch_.Stop();
    }

    float travel_time_seconds = timer_.GetElapsedSeconds() - kStartMovingDelaySeconds;
    float travel_distance = travel_time_seconds * distance_per_second_;

    int num_times_across = travel_distance / distance_;

    float distance_across = travel_distance - (num_times_across * distance_);

    float multiplier;
    if (num_times_across % 2 == 0) {
      // Example: first time across.
      // Going from start to end.
      multiplier = distance_across;
    } else {
      // Going from end to start.
      multiplier = distance_ - distance_across;
    }

    target_->position = start_ + (start_to_end_ * multiplier);
  }

  void OnScenarioDone() override {
    stats_.targets_hit = hit_stopwatch_.GetElapsedSeconds() * 10;
    stats_.shots_taken = shot_stopwatch_.GetElapsedSeconds() * 10;
  }

 private:
  Target* target_ = nullptr;
  glm::vec3 start_;
  glm::vec3 end_;
  glm::vec3 start_to_end_;
  float distance_;
  float distance_per_second_;
  Stopwatch hit_stopwatch_;
  Stopwatch shot_stopwatch_;
};

}  // namespace

std::unique_ptr<Scenario> CreateCenteringScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<CenteringScenario>(def, app);
}

}  // namespace aim

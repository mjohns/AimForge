#include <memory>

#include "aim/common/wall.h"
#include "aim/core/application.h"
#include "aim/scenario/base_scenario.h"
#include "aim/scenario/scenario.h"
#include "glm/gtc/constants.hpp"
#include "glm/vec2.hpp"  // IWYU pragma: keep
#include "glm/vec3.hpp"  // IWYU pragma: keep

namespace aim {
namespace {

class SineScenario : public BaseScenario {
 public:
  explicit SineScenario(const CreateScenarioParams& params)
      : BaseScenario(params), wall_(Wall::ForRoom(params.def.room())) {
    auto d = params.def.sine_def();
    going_left_ = d.going_left();

    height_ = wall_.GetRegionLength(d.height());
    float width = wall_.GetRegionLength(d.width());

    x_scale_ = width / glm::two_pi<float>();
    sine_length_ = GetSineLength(height_, x_scale_);
    t_speed_ = glm::two_pi<float>() / sine_length_;
  }

 protected:
  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void FillInNewTarget(Target* target) override {
    target->wall_position = wall_start_;
  }

  void UpdateTargetPositions() override {
    // Determine if the target needs to change direction
    Target* target = nullptr;
    for (Target* t : target_manager_.GetMutableVisibleTargets()) {
      target = t;
      break;
    }

    if (target == nullptr) {
      return;
    }

    float max_x = (wall_.width * 0.5) - (target->radius * 1.2);
    float min_x = -1 * max_x;
    float current_x = target->wall_position->x;
    if (current_x >= max_x) {
      going_left_ = true;
      int pi_times = current_t_ / glm::two_pi<float>();
      current_t_ -= pi_times * glm::two_pi<float>();
      wall_start_.x = current_x + (x_scale_ * current_t_);
    }
    if (current_x <= min_x) {
      going_left_ = false;
      int pi_times = current_t_ / glm::two_pi<float>();
      current_t_ -= pi_times * glm::two_pi<float>();
      wall_start_.x = current_x - (x_scale_ * current_t_);
    }

    float now_seconds = timer_.GetElapsedSeconds();
    float delta_seconds = now_seconds - last_update_time_;
    last_update_time_ = now_seconds;

    float next_t = current_t_ + (delta_seconds * t_speed_ * target->speed);
    current_t_ = next_t;

    float direction_mult = going_left_ ? -1.0 : 1.0;
    glm::vec2 sine_point(direction_mult * x_scale_ * next_t, height_ * sin(next_t));

    auto point = wall_start_ + sine_point;
    target->wall_position = point;
    target->position = WallPositionToWorldPosition(point, target->radius, def_.room());
  }

 private:
  float GetSineLength(float height, float x_scale) {
    constexpr const float kStep = glm::half_pi<float>() / 2000.0f;
    float distance = 0;

    glm::vec2 prev(0, 0);
    float t = kStep;
    while (true) {
      bool done = false;
      if (t >= glm::half_pi<float>()) {
        done = true;
        t = glm::half_pi<float>();
      }
      glm::vec2 next(x_scale * t, height * sin(t));
      distance += glm::length(next - prev);
      if (done) {
        break;
      }
      prev = next;
      t += kStep;
    }

    return distance * 4;
  }

  Wall wall_;
  glm::vec2 wall_start_{0, 0};
  float last_update_time_ = 0;
  float current_t_ = 0;

  float height_ = 20;
  float x_scale_;
  float sine_length_ = 0;
  float t_speed_ = 0;

  bool going_left_ = false;
};

}  // namespace

std::unique_ptr<Scenario> CreateSineScenario(const CreateScenarioParams& params) {
  return std::make_unique<SineScenario>(params);
}

}  // namespace aim

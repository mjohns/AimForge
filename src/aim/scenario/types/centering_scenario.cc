#include <SDL3/SDL.h>
#include <imgui.h>

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <random>

#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/core/tracking_sound.h"
#include "aim/proto/common.pb.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/settings.pb.h"
#include "aim/scenario/scenario.h"

namespace aim {
namespace {

constexpr const float kStartMovingDelaySeconds = 0.2;

class CenteringScenario : public Scenario {
 public:
  explicit CenteringScenario(const ScenarioDef& def, Application* app)
      : Scenario(def, app),
        start_(ToVec3(def.centering_def().start_position())),
        end_(ToVec3(def.centering_def().end_position())) {
    start_to_end_ = glm::normalize(end_ - start_);
    distance_ = glm::length(start_ - end_);
    initial_distance_offset_ = distance_ * 0.45;
  }

 protected:
  void Initialize() override {
    TargetProfile target_profile = GetNextTargetProfile();
    Target target = GetTargetTemplate(target_profile);
    if (target_profile.has_pill()) {
      target.pill_up =
          glm::normalize(glm::cross(start_to_end_, glm::normalize(start_ - camera_.GetPosition())));
    }

    target.position = start_ + (start_to_end_ * initial_distance_offset_);

    // Look a little in front of the starting position
    glm::vec3 initial_look_at =
        (target.position + (start_to_end_ * (0.07f * distance_))) - camera_.GetPosition();
    PitchYaw pitch_yaw = GetPitchYawFromLookAt(initial_look_at);
    camera_.UpdatePitchYaw(pitch_yaw);

    target = target_manager_.AddTarget(target);
    target_ = target_manager_.GetMutableTarget(target.id);
    AddNewTargetEvent(target);
  }

  ShotType::TypeCase GetDefaultShotType() override {
    return ShotType::kTrackingInvincible;
  }

  void UpdateState(UpdateStateData* data) override {
    if (timer_.GetElapsedSeconds() < kStartMovingDelaySeconds) {
      return;
    }

    if (data->is_click_held) {
      auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
      if (!tracking_sound_) {
        tracking_sound_ = std::make_unique<TrackingSound>(app_);
      }
      shot_stopwatch_.Start();
      if (maybe_hit_target_id.has_value()) {
        hit_stopwatch_.Start();
      } else {
        hit_stopwatch_.Stop();
      }
      tracking_sound_->DoTick(maybe_hit_target_id.has_value());
    } else {
      TrackingHoldDone();
    }

    float travel_time_seconds = timer_.GetElapsedSeconds() - kStartMovingDelaySeconds;
    float travel_distance = travel_time_seconds * target_->speed + initial_distance_offset_;

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

  void OnPause() override {
    TrackingHoldDone();
  }

  void OnScenarioDone() override {
    TrackingHoldDone();
    stats_.targets_hit = hit_stopwatch_.GetElapsedSeconds() * 100;
    stats_.shots_taken = shot_stopwatch_.GetElapsedSeconds() * 100;
  }

 private:
  void TrackingHoldDone() {
    shot_stopwatch_.Stop();
    hit_stopwatch_.Stop();
    tracking_sound_ = {};
  }

  Target* target_ = nullptr;
  glm::vec3 start_;
  glm::vec3 end_;
  glm::vec3 start_to_end_;
  float distance_;
  float initial_distance_offset_;
  Stopwatch hit_stopwatch_;
  Stopwatch shot_stopwatch_;
  std::unique_ptr<TrackingSound> tracking_sound_;
};

}  // namespace

std::unique_ptr<Scenario> CreateCenteringScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<CenteringScenario>(def, app);
}

}  // namespace aim

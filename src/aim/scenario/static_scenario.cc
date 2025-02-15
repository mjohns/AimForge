#include "static_scenario.h"

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
constexpr u64 kPokeBallKillTimeMillis = 50;

bool AreNoneWithinDistanceOnWall(const glm::vec2& p,
                                 float min_distance,
                                 TargetManager* target_manager) {
  for (auto& target : target_manager->GetTargets()) {
    if (!target.hidden) {
      glm::vec2 target_position(target.position.x, target.position.z);
      float distance = glm::length(p - target_position);
      if (distance < min_distance) {
        return false;
      }
    }
  }
  return true;
}

bool AreNoneWithinDistance(const glm::vec3& p, float min_distance, TargetManager* target_manager) {
  for (auto& target : target_manager->GetTargets()) {
    if (!target.hidden) {
      float distance = glm::length(p - target.position);
      if (distance < min_distance) {
        return false;
      }
    }
  }
  return true;
}

glm::vec2 GetRandomPositionInCircle(float max_radius_x, float max_radius_y, Application* app) {
  auto dist_radius = std::uniform_real_distribution<float>(0, max_radius_x);
  auto dist_degrees = std::uniform_real_distribution<float>(0.01, 360);

  float radius = dist_radius(*app->GetRandomGenerator());
  float rotate_radians = glm::radians(dist_degrees(*app->GetRandomGenerator()));

  double cos_angle = cos(rotate_radians);
  double sin_angle = sin(rotate_radians);

  float x = -1 * radius * sin_angle;
  float y_scale = max_radius_y / max_radius_x;
  float y = radius * cos_angle * y_scale;
  return glm::vec2(x, y);
}

float GetRegionLength(const Room& room, const RegionLength& length) {
  if (length.has_absolute_value()) {
    return length.absolute_value();
  }
  if (length.has_x_percent_value()) {
    float width = room.has_circular_room() ? 360 : room.simple_room().width();
    return width * length.x_percent_value();
  }
  if (length.has_y_percent_value()) {
    float height =
        room.has_circular_room() ? room.circular_room().height() : room.simple_room().height();
    return height * length.y_percent_value();
  }
  return 0;
}

std::optional<TargetRegion> GetRegionToUse(const ScenarioDef& def,
                                           TargetManager* target_manager,
                                           Application* app) {
  const TargetPlacementStrategy& strategy = def.static_def().target_placement_strategy();
  if (strategy.regions_size() == 0) {
    return {};
  }
  if (strategy.alternate_regions()) {
    int i = target_manager->GetTargetIdCounter() % strategy.regions_size();
    return strategy.regions(i);
  }

  auto region_chance_dist = std::uniform_real_distribution<float>(0, 1);
  for (const TargetRegion& region : strategy.regions()) {
    float region_chance_roll = region_chance_dist(*app->GetRandomGenerator());
    float percent_chance = region.has_percent_chance() ? region.percent_chance() : 1;
    if (percent_chance >= region_chance_roll) {
      return region;
    }
  }

  return {};
}

// Returns an x/z pair where to place the target on the back wall. For circular rooms x is degrees
// to rotate.
glm::vec2 GetNewCandidateTargetPosition(const ScenarioDef& def,
                                        TargetManager* target_manager,
                                        Application* app) {
  auto maybe_region = GetRegionToUse(def, target_manager, app);
  if (!maybe_region.has_value()) {
    app->logger()->warn("Unable to find target region for scenario {}", def.scenario_id());
    return glm::vec2(0);
  }

  const TargetRegion& region = *maybe_region;
  float x_offset = GetRegionLength(def.room(), region.x_offset());
  float y_offset = GetRegionLength(def.room(), region.y_offset());

  if (region.has_oval()) {
    return GetRandomPositionInCircle(
        x_offset + 0.5 * GetRegionLength(def.room(), region.oval().x_diamter()),
        y_offset + 0.5 * GetRegionLength(def.room(), region.oval().y_diamter()),
        app);
  }
  float max_x = 0.5 * GetRegionLength(def.room(), region.rectangle().x_length());
  float max_y = 0.5 * GetRegionLength(def.room(), region.rectangle().y_length());
  auto distribution_x = std::uniform_real_distribution<float>(-1 * max_x, max_x);
  auto distribution_y = std::uniform_real_distribution<float>(-1 * max_y, max_y);
  return glm::vec2(x_offset + distribution_x(*app->GetRandomGenerator()),
                   y_offset + distribution_y(*app->GetRandomGenerator()));
}

glm::vec3 GetNewTargetPosition(const ScenarioDef& def,
                               TargetManager* target_manager,
                               Application* app) {
  for (int i = 0; i < 30; ++i) {
    glm::vec2 candidate_pos = GetNewCandidateTargetPosition(def, target_manager, app);

    if (def.room().has_circular_room()) {
      glm::vec3 pos;
      pos.z = candidate_pos.y;

      float radius = def.room().circular_room().radius() - (0.7 * def.static_def().target_radius());
      glm::vec2 to_rotate(0, radius);
      glm::vec2 rotated = RotateDegrees(to_rotate, -1 * candidate_pos.x);

      pos.x = rotated.x;
      pos.y = rotated.y;
      if (AreNoneWithinDistance(
              pos, def.static_def().target_placement_strategy().min_distance(), target_manager)) {
        return pos;
      }
    }

    if (def.room().has_simple_room()) {
      // This distance check is done on the 2d wall where the coordinate system has z in the actual
      // room mapped to y.
      if (AreNoneWithinDistanceOnWall(candidate_pos,
                                      def.static_def().target_placement_strategy().min_distance(),
                                      target_manager)) {
        glm::vec3 pos;
        pos.x = candidate_pos.x;
        pos.z = candidate_pos.y;
        float radius = def.static_def().target_radius();
        // Make sure the target does not clip throug wall
        pos.y = -1 * (radius + 0.5);
        return pos;
      }
    }
  }

  app->logger()->warn("Unable to place target in scenario {}", def.scenario_id());
  return glm::vec3();
}

Target GetNewTarget(const ScenarioDef& def, TargetManager* target_manager, Application* app) {
  glm::vec3 pos = GetNewTargetPosition(def, target_manager, app);
  Target t;
  t.position = pos;
  t.radius = def.static_def().target_radius();
  return t;
}

class StaticScenario : public Scenario {
 public:
  explicit StaticScenario(const ScenarioDef& def, Application* app) : Scenario(def, app) {}

 private:
  void AddNewTargetEvent(const Target& target, i32 explicit_frame_number = -1) {
    auto add_event = replay_->add_events();
    add_event->set_frame_number(explicit_frame_number >= 0 ? explicit_frame_number
                                                           : timer_.GetReplayFrameNumber());
    auto add_target = add_event->mutable_add_static_target();
    add_target->set_target_id(target.id);
    *(add_target->mutable_position()) = ToStoredVec3(target.position);
    add_target->set_radius(target.radius);
  }

  void AddKillTargetEvent(u16 target_id) {
    auto event = replay_->add_events();
    event->set_frame_number(timer_.GetReplayFrameNumber());
    event->mutable_kill_target()->set_target_id(target_id);
  }

  void AddRemoveTargetEvent(u16 target_id) {
    auto event = replay_->add_events();
    event->set_frame_number(timer_.GetReplayFrameNumber());
    event->mutable_remove_target()->set_target_id(target_id);
  }

  void AddShotMissedEvent() {
    auto event = replay_->add_events();
    event->set_frame_number(timer_.GetReplayFrameNumber());
    *event->mutable_shot_missed() = ShotMissedEvent();
  }

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
    replay_->set_is_poke_ball(def_.static_def().is_poke_ball());
    replay_->set_replay_fps(timer_.GetReplayFps());

    for (int i = 0; i < def_.static_def().num_targets(); ++i) {
      Target target = target_manager_.AddTarget(GetNewTarget(def_, &target_manager_, app_));
      AddNewTargetEvent(target, 0);
    }
  }

  void UpdateState(UpdateStateData* data) override {
    if (def_.static_def().is_poke_ball()) {
      auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
      if (maybe_hit_target_id.has_value()) {
        u16 hit_target_id = *maybe_hit_target_id;
        bool is_hitting_current_target =
            current_poke_target_id_.has_value() && *(current_poke_target_id_) == hit_target_id;
        if (is_hitting_current_target) {
          // Still targeting the correct target.
          // Has enough time elapsed to kill target?
          u64 now_micros = timer_.GetElapsedMicros();
          u64 age_micros = now_micros - current_poke_start_time_micros_;
          u64 min_age_micros = kPokeBallKillTimeMillis * 1000;
          if (age_micros >= min_age_micros) {
            stats_.targets_hit++;
            stats_.shots_taken++;
            app_->GetSoundManager()->PlayKillSound();
            data->force_render = true;

            auto hit_target_id = *maybe_hit_target_id;
            Target new_target = target_manager_.ReplaceTarget(
                hit_target_id, GetNewTarget(def_, &target_manager_, app_));

            // Add replay events
            AddNewTargetEvent(new_target);
            AddKillTargetEvent(hit_target_id);

            current_poke_target_id_ = {};
            current_poke_start_time_micros_ = 0;
          }
        } else {
          // Starting time on new target.
          current_poke_target_id_ = hit_target_id;
          current_poke_start_time_micros_ = timer_.GetElapsedMicros();
        }
      } else {
        // No current target. Clear.
        current_poke_target_id_ = {};
        current_poke_start_time_micros_ = 0;
      }
    } else {
      if (data->has_click) {
        stats_.shots_taken++;
        auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
        app_->GetSoundManager()->PlayShootSound();
        if (maybe_hit_target_id.has_value()) {
          stats_.targets_hit++;
          app_->GetSoundManager()->PlayKillSound();
          data->force_render = true;

          auto hit_target_id = *maybe_hit_target_id;
          Target new_target = target_manager_.ReplaceTarget(
              hit_target_id, GetNewTarget(def_, &target_manager_, app_));

          // Add replay events
          AddNewTargetEvent(new_target);
          AddKillTargetEvent(hit_target_id);

        } else {
          // Missed shot
          AddShotMissedEvent();
          if (def_.static_def().remove_closest_target_on_miss()) {
            std::optional<u16> target_id_to_remove =
                target_manager_.GetNearestTargetOnStaticWall(camera_, look_at_.front);
            if (target_id_to_remove.has_value()) {
              Target new_target = target_manager_.ReplaceTarget(
                  *target_id_to_remove, GetNewTarget(def_, &target_manager_, app_));
              AddNewTargetEvent(new_target);
              AddRemoveTargetEvent(*target_id_to_remove);
            }
          }
        }
      }
    }
  }

  TargetManager target_manager_;
  std::optional<u16> current_poke_target_id_;
  u64 current_poke_start_time_micros_ = 0;
};

}  // namespace

std::unique_ptr<Scenario> CreateStaticScenario(const ScenarioDef& def, Application* app) {
  return std::make_unique<StaticScenario>(def, app);
}

}  // namespace aim

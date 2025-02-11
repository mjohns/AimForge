#include "static_scenario.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <format>
#include <fstream>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <random>

#include "aim/audio/sound.h"
#include "aim/common/scope_guard.h"
#include "aim/common/time_util.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/core/metronome.h"
#include "aim/graphics/crosshair.h"
#include "aim/proto/common.pb.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/settings.pb.h"
#include "aim/scenario/replay_viewer.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/scenario_timer.h"
#include "aim/scenario/stats_screen.h"
#include "aim/ui/settings_screen.h"

namespace aim {
namespace {
constexpr u64 kPokeBallKillTimeMillis = 50;

bool AreNoneWithinDistance(const glm::vec2& p, float min_distance, TargetManager* target_manager) {
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
    return room.simple_room().width() * length.x_percent_value();
  }
  if (length.has_y_percent_value()) {
    return room.simple_room().height() * length.y_percent_value();
  }
  return 0;
}

// Returns an x/z pair where to place the target on the back wall.
glm::vec2 GetNewTargetPosition(const ScenarioDef& def,
                               TargetManager* target_manager,
                               Application* app) {
  auto region_chance_dist = std::uniform_real_distribution<float>(0, 1);
  std::function<glm::vec2()> get_candidate_pos = [=] { return glm::vec2(0, 0); };
  float height = def.room().simple_room().height();
  float width = def.room().simple_room().width();
  for (const TargetRegion& region : def.static_def().target_placement_strategy().regions()) {
    float region_chance_roll = region_chance_dist(*app->GetRandomGenerator());
    float percent_chance = region.has_percent_chance() ? region.percent_chance() : 1;
    if (percent_chance >= region_chance_roll) {
      get_candidate_pos = [&] {
        // TODO: Use offset position
        if (region.has_oval()) {
          return GetRandomPositionInCircle(
              0.5 * GetRegionLength(def.room(), region.oval().x_diamter()),
              0.5 * GetRegionLength(def.room(), region.oval().y_diamter()),
              app);
        }
        float max_x = 0.5 * GetRegionLength(def.room(), region.rectangle().x_length());
        float max_y = 0.5 * GetRegionLength(def.room(), region.rectangle().y_length());
        auto distribution_x = std::uniform_real_distribution<float>(-1 * max_x, max_x);
        auto distribution_y = std::uniform_real_distribution<float>(-1 * max_y, max_y);
        return glm::vec2(distribution_x(*app->GetRandomGenerator()),
                         distribution_y(*app->GetRandomGenerator()));
      };
      break;
    }
  }

  int max_attempts = 30;
  int attempt_number = 0;
  while (true) {
    ++attempt_number;
    glm::vec2 pos = get_candidate_pos();
    if (attempt_number >= max_attempts ||
        AreNoneWithinDistance(
            pos, def.static_def().target_placement_strategy().min_distance(), target_manager)) {
      return pos;
    }
  }
}

Target GetNewTarget(const ScenarioDef& def, TargetManager* target_manager, Application* app) {
  glm::vec2 pos = GetNewTargetPosition(def, target_manager, app);
  Target t;
  t.position.x = pos.x;
  t.position.z = pos.y;

  t.radius = def.static_def().target_radius();
  // Make sure the target does not clip throug wall
  t.position.y = -1 * (t.radius + 0.5);
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
    app_->GetRenderer()->DrawSimpleStaticRoom(
        def_.room().simple_room(), target_manager_.GetTargets(), look_at_.transform);
  }

  void Initialize() override {
    *replay_->mutable_room() = def_.room();
    replay_->set_is_poke_ball(def_.static_def().is_poke_ball());
    replay_->set_replay_fps(timer_.GetReplayFps());
    *(replay_->mutable_camera_position()) = ToStoredVec3(camera_.GetPosition());

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

NavigationEvent RunStaticScenario(const ScenarioDef& def, Application* app) {
  while (true) {
    StaticScenario scenario(def, app);
    NavigationEvent nav_event = scenario.Run();
    if (nav_event.type != NavigationEventType::RESTART_LAST_SCENARIO) {
      return nav_event;
    }
  }
}

}  // namespace aim

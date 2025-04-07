#include "base_scenario.h"

#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <random>

#include "aim/common/geometry.h"
#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"

namespace aim {
namespace {
constexpr const float kPokeBallKillTimeSeconds = 0.05;

float GetPartialHitValue(const Target& target) {
  if (target.health_seconds > 0) {
    float elapsed_seconds = target.hit_timer.GetElapsedSeconds();
    float value = elapsed_seconds / target.health_seconds;
    return value < 1 ? value : 1;
  }
  return 0;
}

}  // namespace

void BaseScenario::Initialize() {
  int num_targets = def_.target_def().num_targets();
  for (int i = 0; i < num_targets; ++i) {
    Target target = GetNewTarget();
    if (def_.target_def().newest_target_is_ghost() && i == (num_targets - 1)) {
      target.is_ghost = true;
    }

    float stagger_seconds = def_.target_def().stagger_initial_targets_seconds();
    if (stagger_seconds > 0) {
      RunAfterSeconds(i * stagger_seconds, [=]() {
        Target new_target = target_manager_.AddTarget(target);
        AddNewTargetEvent(new_target);
      });
    } else {
      target = target_manager_.AddTarget(target);
      AddNewTargetEvent(target);
    }
  }
}

void BaseScenario::UpdateState(UpdateStateData* data) {
  auto shot_type = GetShotType();
  if (shot_type == ShotType::kTrackingKill || shot_type == ShotType::kTrackingInvincible) {
    HandleTrackingHits(data);
  } else {
    HandleClickHits(data);
  }
  if (def_.target_def().has_remove_target_after_seconds()) {
    std::vector<u16> targets_to_remove;
    for (const Target& target : target_manager_.GetTargets()) {
      if (target.ShouldDraw() && target.remove_after_time_seconds < timer_.GetElapsedSeconds()) {
        if (ShouldCountPartialKills()) {
          stats_.num_hits += GetPartialHitValue(target);
        }
        targets_to_remove.push_back(target.id);
      }
    }
    for (u16 target_id : targets_to_remove) {
      AddNewTargetDuringRun(target_id, /*is_kill=*/false);
    }
  }
  UpdateTargetPositions();
}

void BaseScenario::HandleTrackingHits(UpdateStateData* data) {
  if (data->is_click_held) {
    auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
    if (!tracking_sound_) {
      tracking_sound_ = std::make_unique<TrackingSound>(app_);
    }
    stats_.shot_stopwatch.Start();
    if (maybe_hit_target_id.has_value()) {
      stats_.hit_stopwatch.Start();
    } else {
      stats_.hit_stopwatch.Stop();
    }
    if (GetShotType() == ShotType::kTrackingKill) {
      for (Target& target : target_manager_.GetMutableTargets()) {
        if (maybe_hit_target_id.has_value() && *maybe_hit_target_id == target.id) {
          target.hit_timer.Start();
        } else {
          target.hit_timer.Stop();
        }
        if (target.health_seconds > 0) {
          if (target.hit_timer.GetElapsedSeconds() >= target.health_seconds) {
            stats_.num_hits++;
            PlayKillSound();
            AddNewTargetDuringRun(target.id);
          } else if (target.radius_at_kill.has_value()) {
            float scale = (target.health_seconds - target.hit_timer.GetElapsedSeconds()) /
                          target.health_seconds;
            float radius_diff =
                target.radius_at_kill->start_radius - target.radius_at_kill->end_radius;
            target.radius = target.radius_at_kill->end_radius + scale * radius_diff;
          }
        }
      }
    }
    tracking_sound_->DoTick(maybe_hit_target_id.has_value());
  } else {
    TrackingHoldDone();
  }
}

void BaseScenario::OnScenarioDone() {
  TrackingHoldDone();
  if (ShouldCountPartialKills()) {
    float partial_kills = 0;
    for (Target& target : target_manager_.GetMutableTargets()) {
      partial_kills += GetPartialHitValue(target);
    }
    stats_.num_hits += partial_kills;
  }
}

bool BaseScenario::ShouldCountPartialKills() {
  return GetShotType() == ShotType::kTrackingKill && !def_.shot_type().no_partial_kills();
}

void BaseScenario::TrackingHoldDone() {
  stats_.shot_stopwatch.Stop();
  stats_.hit_stopwatch.Stop();
  tracking_sound_ = {};
  if (GetShotType() == ShotType::kTrackingKill) {
    for (Target& target : target_manager_.GetMutableTargets()) {
      target.hit_timer.Stop();
    }
  }
}

void BaseScenario::OnPause() {
  TrackingHoldDone();
}

void BaseScenario::HandleClickHits(UpdateStateData* data) {
  if (GetShotType() == ShotType::kPoke) {
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
        u64 min_age_micros = def_.shot_type().has_poke_kill_time_seconds()
                                 ? def_.shot_type().poke_kill_time_seconds()
                                 : kPokeBallKillTimeSeconds * 1000000;
        if (age_micros >= min_age_micros) {
          stats_.num_hits++;
          stats_.num_shots++;
          PlayKillSound();
          data->force_render = true;

          auto hit_target_id = *maybe_hit_target_id;
          AddNewTargetDuringRun(hit_target_id);

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
      stats_.num_shots++;
      auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
      PlayShootSound();
      if (maybe_hit_target_id.has_value()) {
        stats_.num_hits++;
        PlayKillSound();
        data->force_render = true;

        auto hit_target_id = *maybe_hit_target_id;
        AddNewTargetDuringRun(hit_target_id);

      } else {
        // Missed shot
        if (def_.target_def().remove_closest_on_miss()) {
          std::optional<u16> target_id_to_remove =
              target_manager_.GetNearestTargetOnMiss(camera_, look_at_.front);
          if (target_id_to_remove.has_value()) {
            AddNewTargetDuringRun(*target_id_to_remove, /*is_kill=*/false);
          }
        }
      }
    }
  }
}

Target BaseScenario::GetNewTarget() {
  Target t = GetTargetTemplate(GetNextTargetProfile());
  FillInNewTarget(&t);
  return t;
}

void BaseScenario::AddNewTargetDuringRun(u16 old_target_id, bool is_kill) {
  Target target = GetNewTarget();
  if (def_.target_def().newest_target_is_ghost()) {
    target_manager_.MarkAllAsNonGhost();
    target.is_ghost = true;
  }

  if (def_.target_def().has_new_target_delay_seconds()) {
    target_manager_.RemoveTarget(old_target_id);
    RunAfterSeconds(def_.target_def().new_target_delay_seconds(), [=]() {
      Target new_target = target;
      if (def_.target_def().has_remove_target_after_seconds()) {
        new_target.remove_after_time_seconds =
            timer_.GetElapsedSeconds() + def_.target_def().remove_target_after_seconds();
      }
      new_target = target_manager_.AddTarget(new_target);
      AddNewTargetEvent(new_target);
    });
  } else {
    target_manager_.RemoveTarget(old_target_id);
    if (def_.target_def().has_remove_target_after_seconds()) {
      target.remove_after_time_seconds =
          timer_.GetElapsedSeconds() + def_.target_def().remove_target_after_seconds();
    }
    target = target_manager_.AddTarget(target);
    AddNewTargetEvent(target);
  }

  if (is_kill) {
    AddKillTargetEvent(old_target_id);
  } else {
    AddRemoveTargetEvent(old_target_id);
  }

  if (target.wall_direction.has_value()) {
    AddMoveLinearTargetEvent(target, *target.wall_direction, target.speed);
  }
  if (target.direction.has_value()) {
    AddMoveLinearTargetEvent(target, *target.direction, target.speed);
  }
}

}  // namespace aim

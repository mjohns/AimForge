#include "base_scenario.h"

#include <memory>
#include <random>

#include "aim/common/geometry.h"
#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/target_placement.h"
#include "glm/gtc/constants.hpp"
#include "glm/mat4x4.hpp"
#include "glm/trigonometric.hpp"
#include "glm/vec3.hpp"

namespace aim {
namespace {
constexpr const float kPokeBallKillTimeSeconds = 0.05;

float GetPartialHitValue(const Target& target) {
  return 1 - target.GetHealthPercent();
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
        Target new_target = target;
        if (new_target.growth_info) {
          new_target.growth_info->start_time_seconds = timer_.GetElapsedSeconds();
        }
        if (def_.target_def().remove_target_after_seconds() > 0) {
          new_target.remove_after_time_seconds =
              timer_.GetElapsedSeconds() + def_.target_def().remove_target_after_seconds();
        }
        new_target = target_manager_.AddTarget(new_target);
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
  std::vector<u16> targets_to_remove;
  if (shot_type == ShotType::kTrackingKill || shot_type == ShotType::kTrackingInvincible) {
    HandleTrackingHits(data, &targets_to_remove);
  } else if (shot_type == ShotType::kTrackingProximity) {
    HandleProximityTrackingHits(data);
  } else {
    HandleClickHits(data);
  }
  if (def_.target_def().remove_target_after_seconds() > 0) {
    for (const Target& target : target_manager_.GetTargets()) {
      if (target.ShouldDraw() && target.remove_after_time_seconds < timer_.GetElapsedSeconds()) {
        targets_to_remove.push_back(target.id);
      }
    }
  }
  // Handle target growth
  float now_seconds = timer_.GetElapsedSeconds();
  for (Target& target : target_manager_.GetMutableTargets()) {
    if (target.growth_info.has_value()) {
      auto g = *target.growth_info;
      float delta_seconds = now_seconds - g.start_time_seconds;
      bool is_growing = delta_seconds < g.grow_time_seconds;
      if (is_growing) {
        // Change radius.
        float radius_range = std::abs(g.start_radius - g.end_radius);
        float rate = radius_range / g.grow_time_seconds;
        if (g.start_radius < g.end_radius) {
          target.radius = g.start_radius + (rate * delta_seconds);
        } else {
          target.radius = g.start_radius - (rate * delta_seconds);
        }
      } else {
        if (delta_seconds >= (g.grow_time_seconds + g.time_at_final_size_seconds)) {
          targets_to_remove.push_back(target.id);
        }
      }
    }
  }

  for (u16 target_id : targets_to_remove) {
    Target* target = target_manager_.GetMutableTarget(target_id);
    if (target != nullptr && ShouldCountPartialKills()) {
      stats_.num_hits += GetPartialHitValue(*target);
    }
    AddNewTargetDuringRun(target_id, /*is_kill=*/false);
  }

  UpdateTargetPositions();
}

void BaseScenario::HandleProximityTrackingHits(UpdateStateData* data) {
  i64 now_micros = timer_.GetElapsedMicros();
  i64 delta_micros = now_micros - last_proximity_tracking_update_time_micros_;
  last_proximity_tracking_update_time_micros_ = now_micros;

  if (data->is_click_held) {
    if (!proximity_tracking_sound_) {
      proximity_tracking_sound_ =
          std::make_unique<ProximityTrackingSound>(settings_.sound(), &app_);
    }
    std::optional<float> normalized_distance_from_center;
    const auto& targets = target_manager_.GetTargets();
    if (targets.size() > 0) {
      const Target& target = targets[0];
      glm::vec3 position = target.position;

      std::optional<float> maybe_distance =
          target.is_pill
              ? GetPillMissedShotDistance(camera_.GetPosition(),
                                          camera_.GetLookAt().front,
                                          position,
                                          target.height - target.radius)
              : GetMissedShotDistance(camera_.GetPosition(), camera_.GetLookAt().front, position);

      if (maybe_distance) {
        float max_distance = target.radius * target.hit_radius_multiplier;
        float value = (max_distance - *maybe_distance) / max_distance;
        if (value > 0) {
          // Max value for score is 750.
          stats_.num_hits += delta_micros * ((value + 0.25) * 0.00001);
          normalized_distance_from_center = 1.0 - value;

          stats_.proximity.hit_micros_100 += delta_micros;
          if (*normalized_distance_from_center <= 0.75) {
            stats_.proximity.hit_micros_75 += delta_micros;
          }
          if (*normalized_distance_from_center <= 0.50) {
            stats_.proximity.hit_micros_50 += delta_micros;
          }
          if (*normalized_distance_from_center <= 0.25) {
            stats_.proximity.hit_micros_25 += delta_micros;
          }
        }
      }
    }

    proximity_tracking_sound_->DoTick(normalized_distance_from_center);
  } else {
    TrackingHoldDone();
  }
}

void BaseScenario::HandleTrackingHits(UpdateStateData* data,
                                      std::vector<u16>* target_ids_to_remove) {
  if (data->is_click_held) {
    auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
    if (!tracking_sound_) {
      tracking_sound_ = std::make_unique<TrackingSound>(settings_.sound(), &app_);
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
          target.StartHitTimer();
          target.is_hit = true;
        } else {
          target.is_hit = false;
          target.StopHitTimer();
        }
        float remove_if_below_health_seconds = def_.shot_type().remove_if_below_health_seconds();
        if (remove_if_below_health_seconds > 0) {
          float remaining_health_seconds = target.GetHealthPercent() * target.health_seconds;
          if (remaining_health_seconds <= remove_if_below_health_seconds) {
            if (!target.kill_sound_played) {
              PlayKillSound();
              target.kill_sound_played = true;
            }
          }
        }
        if (target.health_seconds > 0) {
          float health_percent = target.GetHealthPercent();
          if (health_percent <= 0) {
            stats_.num_hits++;
            stats_.num_kills++;
            if (!target.kill_sound_played) {
              PlayKillSound();
              target.kill_sound_played = true;
            }
            AddNewTargetDuringRun(target.id);
          }
        }
      }
    }
    tracking_sound_->DoTick(maybe_hit_target_id.has_value());
  } else {
    TrackingHoldDone();
  }
  if (GetShotType() == ShotType::kTrackingKill) {
    for (Target& target : target_manager_.GetMutableTargets()) {
      if (target.health_seconds > 0 && target.radius_at_kill.has_value()) {
        float radius_diff = target.radius_at_kill->start_radius - target.radius_at_kill->end_radius;
        float health_percent = target.GetHealthPercent();
        target.radius = target.radius_at_kill->end_radius + health_percent * radius_diff;
      }

      // See if for tracking kill we are no longer on target and it has low enough health to be
      // removed.
      float remove_if_below_health_seconds = def_.shot_type().remove_if_below_health_seconds();
      if (remove_if_below_health_seconds > 0 && !target.is_hit) {
        float remaining_health_seconds = target.GetHealthPercent() * target.health_seconds;
        if (remaining_health_seconds <= remove_if_below_health_seconds) {
          target_ids_to_remove->push_back(target.id);
        }
      }
    }
  }
}

void BaseScenario::OnScenarioDone() {
  stats_.hit_stopwatch.Stop();
  stats_.shot_stopwatch.Stop();
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

bool BaseScenario::ShouldLimitShotRate() {
  return def_.shot_type().click_rate_seconds() > 0;
}

void BaseScenario::TrackingHoldDone() {
  stats_.shot_stopwatch.Stop();
  stats_.hit_stopwatch.Stop();
  tracking_sound_ = {};
  proximity_tracking_sound_ = {};
  if (GetShotType() == ShotType::kTrackingKill) {
    for (Target& target : target_manager_.GetMutableTargets()) {
      target.is_hit = false;
      if (is_done()) {
        target.StopAllTimers();
      } else {
        target.StopHitTimer();
      }
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
        i64 now_micros = timer_.GetElapsedMicros();
        i64 age_micros = now_micros - current_poke_start_time_micros_;
        i64 min_age_micros = (def_.shot_type().has_poke_kill_time_seconds()
                                  ? def_.shot_type().poke_kill_time_seconds()
                                  : kPokeBallKillTimeSeconds) *
                             1000000;
        if (age_micros >= min_age_micros) {
          stats_.num_hits++;
          stats_.num_shots++;
          stats_.num_kills++;
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
    bool shot_fired = data->has_click;
    if (shot_fired && ShouldLimitShotRate()) {
      // See if we should allow the shot.
      i64 shot_rate_micros = def_.shot_type().click_rate_seconds() * 1000000;
      i64 now_micros = timer_.GetElapsedMicros();
      i64 time_since_last_shot = now_micros - last_shot_time_micros_;
      if (time_since_last_shot < shot_rate_micros) {
        shot_fired = false;
      } else {
        // Allow the shot.
        last_shot_time_micros_ = now_micros;
      }
    }

    if (shot_fired) {
      stats_.num_shots++;
      auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at_.front);
      PlayShootSound();
      if (maybe_hit_target_id.has_value()) {
        if (GetShotType() == ShotType::kClickMulti) {
          Target* hit_target = target_manager_.GetMutableTarget(*maybe_hit_target_id);
          stats_.num_hits++;
          hit_target->click_count++;
          PlayHitSound();
          if (hit_target->radius_at_kill.has_value()) {
            float radius_diff =
                hit_target->radius_at_kill->start_radius - hit_target->radius_at_kill->end_radius;
            float health_percent = hit_target->GetHealthPercent();
            hit_target->radius =
                hit_target->radius_at_kill->end_radius + health_percent * radius_diff;
          }
          if (hit_target->click_count >= hit_target->health_clicks) {
            stats_.num_kills++;
            PlayKillSound();
            data->force_render = true;
            auto hit_target_id = *maybe_hit_target_id;
            AddNewTargetDuringRun(hit_target_id);
          }
        } else {
          stats_.num_hits++;
          stats_.num_kills++;
          PlayKillSound();
          data->force_render = true;
          auto hit_target_id = *maybe_hit_target_id;
          AddNewTargetDuringRun(hit_target_id);
        }

      } else {
        // Missed shot
        if (def_.target_def().remove_closest_on_miss()) {
          // TODO(mjohns): Count partial kill for multi click?
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

  if (def_.target_def().new_target_delay_seconds() > 0) {
    target_manager_.RemoveTarget(old_target_id);
    RunAfterSeconds(def_.target_def().new_target_delay_seconds(), [=]() {
      Target new_target = target;
      if (new_target.growth_info) {
        new_target.growth_info->start_time_seconds = timer_.GetElapsedSeconds();
      }
      if (def_.target_def().remove_target_after_seconds() > 0) {
        new_target.remove_after_time_seconds =
            timer_.GetElapsedSeconds() + def_.target_def().remove_target_after_seconds();
      }
      new_target = target_manager_.AddTarget(new_target);
      AddNewTargetEvent(new_target);
    });
  } else {
    target_manager_.RemoveTarget(old_target_id);
    if (def_.target_def().remove_target_after_seconds() > 0) {
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
}

std::optional<StatsRow> BaseScenario::GetStatsRow() {
  ShotType::TypeCase shot_type = GetShotType();
  float score = 0;
  std::optional<StatsExtraInfo> maybe_extra_info;
  switch (shot_type) {
    case ShotType::kTrackingInvincible: {
      stats_.num_hits = stats_.hit_stopwatch.GetElapsedSeconds();
      stats_.num_shots = def_.duration_seconds();
      score = 100 * (stats_.num_hits / stats_.num_shots);
      break;
    }
    case ShotType::kTrackingProximity: {
      float time_normalized_multiplier = 60.0f / def_.duration_seconds();
      score = stats_.num_hits * time_normalized_multiplier;

      float total_micros = def_.duration_seconds() * 1000000;
      stats_.num_shots = def_.duration_seconds();
      stats_.num_hits = (stats_.proximity.hit_micros_100 / total_micros) * stats_.num_shots;

      StatsExtraInfo extra_info;
      extra_info.set_num_hits_75((stats_.proximity.hit_micros_75 / total_micros) *
                                 stats_.num_shots);
      extra_info.set_num_hits_50((stats_.proximity.hit_micros_50 / total_micros) *
                                 stats_.num_shots);
      extra_info.set_num_hits_25((stats_.proximity.hit_micros_25 / total_micros) *
                                 stats_.num_shots);
      maybe_extra_info = extra_info;
      break;
    }
    case ShotType::kTrackingKill: {
      // Make sure to take the score from num hits before changing hits/shots to tracking on/off
      // time so that percentage shows up like for normal tracking invincible scenarios.
      score = stats_.num_hits;
      stats_.num_hits = stats_.hit_stopwatch.GetElapsedSeconds();
      stats_.num_shots = stats_.shot_stopwatch.GetElapsedSeconds();
      break;
    }
    case ShotType::kClickSingle:
    case ShotType::kClickMulti: {
      // Default clicking scoring
      float hit_percent = stats_.num_hits / stats_.num_shots;
      // float duration_modifier = 60.0f / def_.duration_seconds();
      float accuracy_penalty = 1.0 - sqrt(hit_percent);
      if (def_.has_accuracy_penalty_modifier()) {
        accuracy_penalty *= def_.accuracy_penalty_modifier();
      }
      score = stats_.num_hits * (1 - accuracy_penalty);
      break;
    }
    case ShotType::kPoke:
      score = stats_.num_kills;
      break;
    default:
      break;
  }

  StatsRow stats_row;
  stats_row.cm_per_360 = effective_cm_per_360_;
  stats_row.num_hits = stats_.num_hits;
  stats_row.num_shots = stats_.num_shots;
  stats_row.score = score;
  stats_row.extra_info = maybe_extra_info;
  return stats_row;
}

}  // namespace aim

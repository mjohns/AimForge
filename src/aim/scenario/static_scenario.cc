#include "static_scenario.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <flatbuffers/flatbuffers.h>
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
#include "aim/fbs/common_generated.h"
#include "aim/fbs/replay_generated.h"
#include "aim/fbs/settings_generated.h"
#include "aim/graphics/crosshair.h"
#include "aim/scenario/replay_viewer.h"
#include "aim/scenario/scenario_timer.h"
#include "aim/scenario/stats_screen.h"
#include "aim/ui/settings_screen.h"

namespace aim {
namespace {
constexpr u64 kPokeBallKillTimeMillis = 50;

Camera GetInitialCamera(const StaticScenarioParams& params) {
  return Camera(glm::vec3(0, -100.0f, 0));
}

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

// Returns an x/z pair where to place the target on the back wall.
glm::vec2 GetNewTargetPosition(const StaticScenarioParams& params,
                               const TargetPlacementParams& placement_params,
                               TargetManager* target_manager,
                               Application* app) {
  auto region_chance_dist = std::uniform_real_distribution<float>(0, 1);
  std::function<glm::vec2()> get_candidate_pos = [=] { return glm::vec2(0, 0); };
  for (const TargetRegion& region : placement_params.regions) {
    float region_chance_roll = region_chance_dist(*app->GetRandomGenerator());
    if (region.percent_chance >= region_chance_roll) {
      get_candidate_pos = [&] {
        if (region.x_circle_percent > 0) {
          return GetRandomPositionInCircle(0.5 * params.room_width * region.x_circle_percent,
                                           0.5 * params.room_height * region.y_circle_percent,
                                           app);
        }
        float max_x = 0.5 * params.room_width * region.x_percent;
        float max_y = 0.5 * params.room_height * region.y_percent;
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
        AreNoneWithinDistance(pos, placement_params.min_distance, target_manager)) {
      return pos;
    }
  }
}

Target GetNewTarget(const StaticScenarioParams& params,
                    TargetManager* target_manager,
                    Application* app) {
  glm::vec2 pos = GetNewTargetPosition(params, params.target_placement, target_manager, app);
  Target t;
  t.position.x = pos.x;
  t.position.z = pos.y;
  // Make sure the target does not clip throug wall
  t.position.y = -1 * (params.target_radius + 0.5);
  t.radius = params.target_radius;
  return t;
}

}  // namespace

StaticScenario::StaticScenario(const StaticScenarioParams& params, Application* app)
    : params_(params),
      app_(app),
      replay_(std::make_unique<StaticReplayT>()),
      camera_(GetInitialCamera(params)),
      metronome_(params.metronome_bpm, app),
      timer_(replay_frames_per_second_) {}

NavigationEvent StaticScenario::RunScenario(const StaticScenarioParams& params, Application* app) {
  while (true) {
    StaticScenario scenario(params, app);
    NavigationEvent nav_event = scenario.Run();
    if (nav_event.type != NavigationEventType::RESTART_LAST_SCENARIO) {
      return nav_event;
    }
  }
}

NavigationEvent StaticScenario::Run() {
  ScreenInfo screen = app_->GetScreenInfo();
  glm::mat4 projection = GetPerspectiveTransformation(screen);
  float dpi = app_->GetSettingsManager()->GetDpi();
  SettingsT settings = app_->GetSettingsManager()->GetCurrentSettings();
  float radians_per_dot = CmPer360ToRadiansPerDot(settings.cm_per_360, dpi);

  StaticReplayT& replay = *(replay_);
  replay.wall_height = params_.room_height;
  replay.wall_width = params_.room_width;
  replay.is_poke_ball = params_.is_poke_ball;
  u16 replay_frames_per_second = 150;
  replay.frames_per_second = replay_frames_per_second;
  replay.camera_position = ToStoredVec3Ptr(camera_.GetPosition());

  for (int i = 0; i < params_.num_targets; ++i) {
    Target target = target_manager_.AddTarget(GetNewTarget(params_, &target_manager_, app_));

    // Add replay event
    auto add_target = std::make_unique<AddTargetEventT>();
    add_target->target_id = target.id;
    add_target->frame_number = 0;
    add_target->position = ToStoredVec3Ptr(target.position);
    add_target->radius = target.radius;
    replay.add_target_events.push_back(std::move(add_target));
  }

  LookAtInfo look_at;

  SDL_GL_SetSwapInterval(0);  // Disable vsync
  SDL_SetWindowRelativeMouseMode(app_->GetSdlWindow(), true);

  app_->GetRenderer()->SetProjection(projection);

  // Main loop
  bool stop_scenario = false;
  bool show_settings = false;
  while (!stop_scenario) {
    if (show_settings) {
      // Need to pause.
      timer_.Pause();
      SettingsScreen settings_screen;
      auto nav_event = settings_screen.Run(app_);
      if (nav_event.IsNotDone()) {
        return nav_event;
      }
      show_settings = false;

      // Need to refresh everything based on settings change
      settings = app_->GetSettingsManager()->GetCurrentSettings();
      radians_per_dot = CmPer360ToRadiansPerDot(settings.cm_per_360, dpi);
      timer_.Resume();
      SDL_GL_SetSwapInterval(0);  // Disable vsync
      SDL_SetWindowRelativeMouseMode(app_->GetSdlWindow(), true);
    }

    timer_.OnStartFrame();

    if (timer_.IsNewReplayFrame()) {
      // Store the look at vector before the mouse updates for the old frame.
      replay.pitch_yaw_pairs.push_back(PitchYaw(camera_.GetPitch(), camera_.GetYaw()));
    }

    if (timer_.GetElapsedSeconds() >= params_.duration_seconds) {
      stop_scenario = true;
      continue;
    }

    SDL_Event event;
    bool has_click = false;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return NavigationEvent::Exit();
      }
      if (event.type == SDL_EVENT_MOUSE_MOTION) {
        camera_.Update(event.motion.xrel, event.motion.yrel, radians_per_dot);
      }
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        has_click = true;
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_R) {
          return NavigationEvent::RestartLastScenario();
        }
        if (keycode == SDLK_ESCAPE) {
          show_settings = true;
        }
      }
    }

    // Update state

    metronome_.DoTick(timer_.GetElapsedMicros());
    bool force_render = false;
    look_at = camera_.GetLookAt();

    if (params_.is_poke_ball) {
      auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at.front);
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
            force_render = true;

            auto hit_target_id = *maybe_hit_target_id;
            Target new_target = target_manager_.ReplaceTarget(
                hit_target_id, GetNewTarget(params_, &target_manager_, app_));

            // Add replay events
            auto add_target = std::make_unique<AddTargetEventT>();
            add_target->target_id = new_target.id;
            add_target->frame_number = timer_.GetReplayFrameNumber();
            add_target->position = ToStoredVec3Ptr(new_target.position);
            add_target->radius = new_target.radius;
            replay.add_target_events.push_back(std::move(add_target));

            auto hit_target = std::make_unique<HitTargetEventT>();
            hit_target->target_id = hit_target_id;
            hit_target->frame_number = timer_.GetReplayFrameNumber();
            replay.hit_target_events.push_back(std::move(hit_target));

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
      if (has_click) {
        stats_.shots_taken++;
        auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at.front);
        app_->GetSoundManager()->PlayShootSound();
        if (maybe_hit_target_id.has_value()) {
          stats_.targets_hit++;
          app_->GetSoundManager()->PlayKillSound();
          force_render = true;

          auto hit_target_id = *maybe_hit_target_id;
          Target new_target = target_manager_.ReplaceTarget(
              hit_target_id, GetNewTarget(params_, &target_manager_, app_));

          // Add replay events
          auto add_target = std::make_unique<AddTargetEventT>();
          add_target->target_id = new_target.id;
          add_target->frame_number = timer_.GetReplayFrameNumber();
          add_target->position = ToStoredVec3Ptr(new_target.position);
          add_target->radius = new_target.radius;
          replay.add_target_events.push_back(std::move(add_target));

          auto hit_target = std::make_unique<HitTargetEventT>();
          hit_target->target_id = hit_target_id;
          hit_target->frame_number = timer_.GetReplayFrameNumber();
          replay.hit_target_events.push_back(std::move(hit_target));
        } else {
          // Missed shot
          auto miss_target = std::make_unique<MissTargetEventT>();
          miss_target->frame_number = timer_.GetReplayFrameNumber();
          replay.miss_target_events.push_back(std::move(miss_target));
          if (params_.remove_closest_target_on_miss) {
            std::optional<u16> target_id_to_remove =
                target_manager_.GetNearestTargetOnStaticWall(camera_, look_at.front);
            if (target_id_to_remove.has_value()) {
              Target new_target = target_manager_.ReplaceTarget(
                  *target_id_to_remove, GetNewTarget(params_, &target_manager_, app_));

              auto add_target = std::make_unique<AddTargetEventT>();
              add_target->target_id = new_target.id;
              add_target->frame_number = timer_.GetReplayFrameNumber();
              add_target->position = ToStoredVec3Ptr(new_target.position);
              add_target->radius = new_target.radius;
              replay.add_target_events.push_back(std::move(add_target));

              auto remove_target = std::make_unique<RemoveTargetEventT>();
              remove_target->target_id = *target_id_to_remove;
              remove_target->frame_number = timer_.GetReplayFrameNumber();
              replay.remove_target_events.push_back(std::move(remove_target));
            }
          }
        }
      }
    }

    // Render if forced or if the last render was over ~1ms ago.
    bool do_render = force_render || timer_.LastFrameRenderedMicrosAgo() > 1200;
    if (!do_render) {
      continue;
    }

    timer_.OnStartRender();
    auto end_render_guard = ScopeGuard::Create([&] { timer_.OnEndRender(); });

    ImDrawList* draw_list = app_->StartFullscreenImguiFrame();
    DrawCrosshair(*settings.crosshair, screen, draw_list);

    float elapsed_seconds = timer_.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    if (app_->StartRender()) {
      app_->GetRenderer()->DrawSimpleStaticRoom(
          params_.room_height, params_.room_width, target_manager_.GetTargets(), look_at.transform);
      app_->FinishRender();
    }
  }

  if (stats_.shots_taken == 0) {
    return NavigationEvent::GoHome();
  }

  stats_.hit_percent = stats_.targets_hit / (float)stats_.shots_taken;
  float duration_modifier = 60.0f / params_.duration_seconds;
  stats_.score = stats_.targets_hit * 10 * sqrt(stats_.hit_percent) * duration_modifier;

  StatsRow stats_row;
  stats_row.cm_per_360 = settings.cm_per_360;
  stats_row.num_hits = stats_.targets_hit;
  stats_row.num_kills = stats_.targets_hit;
  stats_row.num_shots = stats_.shots_taken;
  stats_row.score = stats_.score;
  app_->GetStatsDb()->AddStats(params_.scenario_id, &stats_row);

  StatsScreen stats_screen(
      params_.scenario_id, stats_row.stats_id, std::move(replay_), stats_, app_);
  return stats_screen.Run();
}

}  // namespace aim

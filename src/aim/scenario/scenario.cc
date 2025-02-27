#include "scenario.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <algorithm>
#include <format>
#include <fstream>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>

#include "aim/common/scope_guard.h"
#include "aim/common/times.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/core/metronome.h"
#include "aim/graphics/crosshair.h"
#include "aim/proto/common.pb.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/settings.pb.h"
#include "aim/scenario/scenario_timer.h"
#include "aim/scenario/screens.h"
#include "aim/scenario/types/scenario_types.h"
#include "aim/ui/ui_screen.h"

namespace aim {
namespace {
constexpr const u16 kReplayFps = 240;

}  // namespace

Scenario::Scenario(const ScenarioDef& def, Application* app)
    : def_(def),
      app_(app),
      timer_(kReplayFps),
      camera_(Camera(
          def.room().start_pitch(), def.room().start_yaw(), ToVec3(def.room().camera_position()))),
      replay_(std::make_unique<Replay>()),
      target_manager_(def.room()) {
  theme_ = app->settings_manager()->GetCurrentTheme();
}

NavigationEvent Scenario::Run() {
  if (replay_) {
    *replay_->mutable_room() = def_.room();
    replay_->set_replay_fps(timer_.GetReplayFps());
  }
  Initialize();

  return Resume();
}

void Scenario::RunAfterSeconds(float delay_seconds, std::function<void()>&& fn) {
  for (auto& task : delayed_tasks_) {
    if (!task.fn) {
      task.fn = std::move(fn);
      task.run_time_seconds = timer_.GetElapsedSeconds() + delay_seconds;
      return;
    }
  }

  delayed_tasks_.push_back({});
  DelayedTask& task = delayed_tasks_.back();
  task.fn = std::move(fn);
  task.run_time_seconds = timer_.GetElapsedSeconds() + delay_seconds;
}

void Scenario::RefreshState() {
  SDL_GL_SetSwapInterval(0);  // Disable vsync
  SDL_SetWindowRelativeMouseMode(app_->sdl_window(), true);

  settings_ = app_->settings_manager()->GetCurrentSettingsForScenario(def_.scenario_id());
  glm::mat4 projection = GetPerspectiveTransformation(app_->screen_info());
  app_->renderer()->SetProjection(projection);

  float dpi = app_->settings_manager()->GetDpi();
  metronome_ = std::make_unique<Metronome>(settings_.metronome_bpm(), app_);
  radians_per_dot_ = CmPer360ToRadiansPerDot(settings_.cm_per_360(), dpi);
  is_click_held_ = false;
  crosshair_ = settings_.crosshair();
  theme_ = app_->settings_manager()->GetCurrentTheme();
}

NavigationEvent Scenario::Resume() {
  if (is_done_) {
    StatsScreen stats_screen(def_.scenario_id(), stats_id_, app_);
    return stats_screen.Run(replay_.get());
  }

  RefreshState();

  bool stop_scenario = false;
  bool is_adjusting_crosshair = false;
  std::optional<QuickSettingsType> show_settings;
  while (!stop_scenario) {
    if (!app_->has_input_focus()) {
      // Pause the scenario if user alt-tabs etc.
      return NavigationEvent::Done();
    }
    if (show_settings.has_value()) {
      // Need to pause.
      timer_.Pause();
      OnPause();
      NavigationEvent nav_event;
      nav_event = CreateQuickSettingsScreen(def_.scenario_id(), *show_settings, app_)->Run();
      if (nav_event.IsNotDone()) {
        return nav_event;
      }
      show_settings = {};
      // Need to refresh everything based on settings change
      RefreshState();
      timer_.Resume();
    }

    timer_.OnStartFrame();

    if (timer_.IsNewReplayFrame()) {
      // Store the look at vector before the mouse updates for the old frame.
      if (replay_) {
        replay_->add_pitch_yaws(camera_.GetPitch());
        replay_->add_pitch_yaws(camera_.GetYaw());
      }
    }

    if (timer_.GetElapsedSeconds() >= def_.duration_seconds()) {
      stop_scenario = true;
      continue;
    }

    SDL_Event event;
    UpdateStateData update_data;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        throw ApplicationExitException();
      }
      if (event.type == SDL_EVENT_MOUSE_MOTION) {
        camera_.Update(event.motion.xrel, event.motion.yrel, radians_per_dot_);
      }
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          update_data.has_click = true;
          is_click_held_ = true;
        }
      }
      if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          update_data.has_click_up = true;
          is_click_held_ = false;
        }
      }
      if (is_adjusting_crosshair) {
        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
          if (event.wheel.y != 0) {
            crosshair_.set_size(crosshair_.size() + event.wheel.y);
          }
        }
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_R) {
          return NavigationEvent::RestartLastScenario();
        }
        if (keycode == SDLK_S) {
          show_settings = QuickSettingsType::DEFAULT;
        }
        if (keycode == SDLK_B) {
          show_settings = QuickSettingsType::METRONOME;
        }
        if (keycode == SDLK_C) {
          is_adjusting_crosshair = true;
        }
        if (keycode == SDLK_ESCAPE) {
          return NavigationEvent::Done();
        }
      }
      if (event.type == SDL_EVENT_KEY_UP) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_C) {
          is_adjusting_crosshair = false;
          Settings* current_settings = app_->settings_manager()->GetMutableCurrentSettings();
          if (current_settings != nullptr) {
            *current_settings->mutable_crosshair() = crosshair_;
            app_->settings_manager()->MarkDirty();
            app_->settings_manager()->MaybeFlushToDisk(def_.scenario_id());
            RefreshState();
          }
        }
      }
      OnEvent(event);
    }

    // Update state

    if (metronome_) {
      metronome_->DoTick(timer_.GetElapsedMicros());
    }
    look_at_ = camera_.GetLookAt();

    update_data.is_click_held = is_click_held_;
    for (auto& task : delayed_tasks_) {
      if (task.fn && task.run_time_seconds < timer_.GetElapsedSeconds()) {
        task.fn();
        task.fn = {};
        task.run_time_seconds = -1;
      }
    }
    UpdateState(&update_data);
    num_state_updates_++;

    // Render if forced or if the last render was over ~1ms ago.
    bool do_render = update_data.force_render || timer_.LastFrameRenderedMicrosAgo() > 1200;
    if (!do_render) {
      continue;
    }

    timer_.OnStartRender();
    auto end_render_guard = ScopeGuard::Create([&] { timer_.OnEndRender(); });

    ImDrawList* draw_list = app_->StartFullscreenImguiFrame();
    DrawCrosshair(crosshair_, theme_, app_->screen_info(), draw_list);

    float elapsed_seconds = timer_.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::Text("cm/360: %.0f", settings_.cm_per_360());
    if (settings_.metronome_bpm() > 0) {
      ImGui::Text("metronome bpm: %.0f", settings_.metronome_bpm());
    }

    // ~ Around 450k
    // ImGui::Text("ups: %.1f", num_state_updates_ / timer_.GetElapsedSeconds());

    ImGui::End();

    if (app_->StartRender(ImVec4(0, 0, 0, 1))) {
      app_->renderer()->DrawScenario(
          def_.room(), theme_, target_manager_.GetTargets(), look_at_.transform);
      app_->FinishRender();
    }
  }

  OnScenarioDone();
  is_done_ = true;

  stats_.hit_percent = stats_.targets_hit / (float)stats_.shots_taken;
  float duration_modifier = 60.0f / def_.duration_seconds();
  stats_.score = stats_.targets_hit * 10 * sqrt(stats_.hit_percent) * duration_modifier;

  StatsRow stats_row;
  stats_row.cm_per_360 = settings_.cm_per_360();
  stats_row.num_hits = stats_.targets_hit;
  stats_row.num_kills = stats_.targets_hit;
  stats_row.num_shots = stats_.shots_taken;
  stats_row.score = stats_.score;
  app_->stats_db()->AddStats(def_.scenario_id(), &stats_row);

  stats_id_ = stats_row.stats_id;

  PlaylistRun* playlist_run = app_->playlist_manager()->GetMutableCurrentRun();
  if (playlist_run != nullptr && playlist_run->IsCurrentIndexValid()) {
    PlaylistItemProgress* progress = playlist_run->GetMutableCurrentPlaylistItemProgress();
    if (def_.scenario_id() == progress->item.scenario()) {
      progress->runs_done++;
      if (progress->item.auto_next()) {
        return NavigationEvent::PlaylistNext();
      }
    }
  }

  StatsScreen stats_screen(def_.scenario_id(), stats_row.stats_id, app_);
  return stats_screen.Run(replay_.get());
}

void Scenario::AddNewTargetEvent(const Target& target) {
  if (!replay_) {
    return;
  }
  auto add_event = replay_->add_events();
  add_event->set_frame_number(timer_.GetReplayFrameNumber());
  auto add_target = add_event->mutable_add_target();
  add_target->set_target_id(target.id);
  *(add_target->mutable_position()) = ToStoredVec3(target.position);
  add_target->set_radius(target.radius);
}

void Scenario::AddKillTargetEvent(u16 target_id) {
  if (!replay_) {
    return;
  }
  auto event = replay_->add_events();
  event->set_frame_number(timer_.GetReplayFrameNumber());
  event->mutable_kill_target()->set_target_id(target_id);
}

void Scenario::AddRemoveTargetEvent(u16 target_id) {
  if (!replay_) {
    return;
  }
  auto event = replay_->add_events();
  event->set_frame_number(timer_.GetReplayFrameNumber());
  event->mutable_remove_target()->set_target_id(target_id);
}

void Scenario::AddShotFiredEvent() {
  if (!replay_) {
    return;
  }
  auto event = replay_->add_events();
  event->set_frame_number(timer_.GetReplayFrameNumber());
  *event->mutable_shot_fired() = ShotFiredEvent();
}

void Scenario::AddMoveLinearTargetEvent(const Target& target,
                                        const glm::vec2& direction,
                                        float distance_per_second) {
  if (!replay_) {
    return;
  }
  // Translate from 2d direction on static wall.
  glm::vec3 dir3(direction.x, 0, direction.y);
  AddMoveLinearTargetEvent(target, dir3, distance_per_second);
}

void Scenario::AddMoveLinearTargetEvent(const Target& target,
                                        const glm::vec3& direction,
                                        float distance_per_second) {
  if (!replay_) {
    return;
  }
  auto event = replay_->add_events();
  event->set_frame_number(timer_.GetReplayFrameNumber());
  MoveLinearTargetEvent t;
  t.set_target_id(target.id);
  *t.mutable_starting_position() = ToStoredVec3(target.position);
  *t.mutable_direction() = ToStoredVec3(direction);
  t.set_distance_per_second(distance_per_second);
  *event->mutable_move_linear_target() = t;
}

void Scenario::PlayShootSound() {
  app_->sound_manager()->PlayShootSound();
  AddShotFiredEvent();
}

void Scenario::PlayMissSound() {
  app_->sound_manager()->PlayShootSound();
}

void Scenario::PlayKillSound() {
  app_->sound_manager()->PlayKillSound();
}

TargetProfile Scenario::GetNextTargetProfile() {
  return target_manager_.GetTargetProfile(def_.target_def(), app_->random_generator());
}

Target Scenario::GetTargetTemplate(const TargetProfile& profile) {
  Target target;
  target.last_update_time_seconds = timer_.GetElapsedSeconds();
  target.radius = GetJitteredValue(
      profile.target_radius(), profile.target_radius_jitter(), app_->random_generator());
  target.speed =
      GetJitteredValue(profile.speed(), profile.speed_jitter(), app_->random_generator());
  if (profile.has_pill()) {
    target.is_pill = true;
    target.height = profile.pill().height();
  }
  return target;
}

std::unique_ptr<Scenario> CreateScenario(const ScenarioDef& def, Application* app) {
  switch (def.type_case()) {
    case ScenarioDef::kStaticDef:
      return CreateStaticScenario(def, app);
    case ScenarioDef::kCenteringDef:
      return CreateCenteringScenario(def, app);
    case ScenarioDef::kBarrelDef:
      return CreateBarrelScenario(def, app);
    default:
      break;
  }
  return {};
}

}  // namespace aim

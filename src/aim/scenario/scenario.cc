#include "scenario.h"

#include <SDL3/SDL.h>
#include <absl/strings/ascii.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <algorithm>
#include <format>
#include <fstream>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>

#include "aim/common/imgui_ext.h"
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
constexpr const u64 kTargetRenderFps = 615;

}  // namespace

Scenario::Scenario(const ScenarioDef& def, Application* app)
    : def_(def),
      app_(app),
      timer_(kReplayFps),
      camera_(Camera(
          def.room().start_pitch(), def.room().start_yaw(), ToVec3(def.room().camera_position()))),
      target_manager_(def.room()) {
  theme_ = app->settings_manager()->GetCurrentTheme();
  max_render_age_micros_ = (1 / (float)(kTargetRenderFps + 1)) * 1000 * 1000;
}

NavigationEvent Scenario::Run() {
  if (ShouldRecordReplay()) {
    replay_ = google::protobuf::Arena::Create<Replay>(&replay_arena_);
    *replay_->mutable_room() = def_.room();
    replay_->set_replay_fps(timer_.GetReplayFps());
  }
  return Resume();
}

void Scenario::RunAfterSeconds(float delay_seconds, std::function<void()>&& fn) {
  for (auto& task : delayed_tasks_) {
    if (!task.fn.has_value()) {
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
  if (has_started_) {
    app_->DisableVsync();
  } else {
    app_->EnableVsync();
  }

  SDL_SetWindowRelativeMouseMode(app_->sdl_window(), true);

  settings_ = app_->settings_manager()->GetCurrentSettingsForScenario(def_.scenario_id());
  projection_ = GetPerspectiveTransformation(app_->screen_info());

  float dpi = app_->settings_manager()->GetDpi();
  metronome_ = std::make_unique<Metronome>(settings_.metronome_bpm(), app_);
  radians_per_dot_ = CmPer360ToRadiansPerDot(settings_.cm_per_360(), dpi);
  is_click_held_ = false;
  crosshair_ = app_->settings_manager()->GetCurrentCrosshair();
  crosshair_size_ = settings_.crosshair_size();
  theme_ = app_->settings_manager()->GetCurrentTheme();
  if (ShouldAutoHold()) {
    is_click_held_ = true;
  }
}

bool Scenario::ShouldAutoHold() {
  if (!settings_.auto_hold_tracking()) {
    return false;
  }
  auto type = GetShotType();
  return type == ShotType::kTrackingInvincible || type == ShotType::kTrackingKill;
}

NavigationEvent Scenario::RunWaitingScreenAndThenStart() {
  RefreshState();

  if (settings_.disable_click_to_start()) {
    has_started_ = true;
    Initialize();
    return ResumeInternal();
  }

  bool is_adjusting_crosshair = false;
  bool save_crosshair = false;
  std::optional<QuickSettingsType> show_settings;
  std::string show_settings_release_key;
  look_at_ = camera_.GetLookAt();
  timer_.StartLoop();
  bool running = true;
  while (running) {
    if (!app_->has_input_focus()) {
      return NavigationEvent::Done();
    }
    if (save_crosshair) {
      DoneAdjustingCrosshairSize();
      save_crosshair = false;
    }
    if (show_settings.has_value()) {
      NavigationEvent nav_event;
      nav_event = CreateQuickSettingsScreen(
                      def_.scenario_id(), *show_settings, show_settings_release_key, app_)
                      ->Run();
      if (nav_event.IsNotDone()) {
        return nav_event;
      }
      show_settings = {};
      // Need to refresh everything based on settings change
      RefreshState();
    }

    timer_.OnStartFrame();

    SDL_Event event;
    UpdateStateData update_data;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        throw ApplicationExitException();
      }
      if (is_adjusting_crosshair) {
        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
          if (event.wheel.y != 0) {
            crosshair_size_ += event.wheel.y;
          }
        }
      }
      if (IsMappableKeyDownEvent(event)) {
        std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().fire())) {
          running = false;
        }
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().quick_settings())) {
          show_settings = QuickSettingsType::DEFAULT;
          show_settings_release_key = event_name;
        }
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().quick_metronome())) {
          show_settings = QuickSettingsType::METRONOME;
          show_settings_release_key = event_name;
        }
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().adjust_crosshair_size())) {
          is_adjusting_crosshair = true;
        }
      }
      if (IsEscapeKeyDown(event)) {
        return NavigationEvent::Done();
      }
      if (IsMappableKeyUpEvent(event)) {
        std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().adjust_crosshair_size())) {
          is_adjusting_crosshair = false;
          save_crosshair = true;
        }
      }
    }

    bool do_render = timer_.LastFrameRenderStartedMicrosAgo() > max_render_age_micros_;
    if (!do_render) {
      continue;
    }

    timer_.OnStartRender();
    auto end_render_guard = ScopeGuard::Create([&] { timer_.OnEndRender(); });

    ImDrawList* draw_list = app_->StartFullscreenImguiFrame();
    DrawCrosshair(crosshair_, crosshair_size_, theme_, app_->screen_info(), draw_list);

    ImGui::Text("%s", def_.scenario_id().c_str());
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::Text("metronome bpm: %.0f", settings_.metronome_bpm());
    ImGui::Text("theme: %s", settings_.theme_name().c_str());
    ImGui::Text("cm/360: %.0f", settings_.cm_per_360());

    ImGui::PushStyleColor(ImGuiCol_Text, ToImCol32(theme_.target_color()));
    {
      auto bold = app_->font_manager()->UseLargeBold();
      std::string message = "Click to Start";
      ImVec2 text_size = ImGui::CalcTextSize(message.c_str());
      ImGui::SetCursorPosX(app_->screen_info().center.x - text_size.x * 0.5);
      ImGui::SetCursorPosY(app_->screen_info().center.y - text_size.y * 1.75);
      ImGui::Text(message);
    }

    ImVec2 text_size = ImGui::CalcTextSize(def_.scenario_id().c_str());
    ImGui::SetCursorPosX(app_->screen_info().center.x - text_size.x * 0.5);
    ImGui::SetCursorPosY(app_->screen_info().center.y + text_size.y * 1);
    ImGui::Text("%s", def_.scenario_id().c_str());

    std::string message = std::format("cm/360: {}", MaybeIntToString(settings_.cm_per_360(), 1));
    text_size = ImGui::CalcTextSize(message.c_str());
    ImGui::SetCursorPosX(app_->screen_info().center.x - text_size.x * 0.5);
    ImGui::SetCursorPosY(app_->screen_info().center.y + text_size.y * 2);
    ImGui::Text("%s", message.c_str());

    ImGui::PopStyleColor();
    ImGui::End();

    RenderContext ctx;
    if (app_->StartRender(&ctx)) {
      app_->renderer()->DrawScenario(projection_,
                                     def_.room(),
                                     theme_,
                                     target_manager_.GetTargets(),
                                     look_at_.transform,
                                     &ctx,
                                     timer_.run_stopwatch(),
                                     &current_times_);
      current_times_.render_imgui_start = timer_.GetElapsedMicros();
      app_->FinishRender(&ctx);
      current_times_.render_imgui_end = timer_.GetElapsedMicros();
    }
  }

  // Scenario has started.
  SDL_Delay(200);
  has_started_ = true;
  Initialize();
  return ResumeInternal();
}

NavigationEvent Scenario::Resume() {
  if (!has_started_) {
    return RunWaitingScreenAndThenStart();
  } else {
    return ResumeInternal();
  }
}

NavigationEvent Scenario::ResumeInternal() {
  if (is_done_) {
    StatsScreen stats_screen(def_.scenario_id(), stats_id_, app_, perf_stats_);
    return stats_screen.Run(replay_);
  }

  RefreshState();

  u64 loop_count = 0;
  bool stop_scenario = false;
  bool is_adjusting_crosshair = false;
  bool save_crosshair = false;
  std::optional<QuickSettingsType> show_settings;
  std::string show_settings_release_key;
  timer_.StartLoop();
  timer_.ResumeRun();
  while (!stop_scenario) {
    loop_count++;
    if (loop_count % 50000 == 0) {
      state_updates_per_second_ = (num_state_updates_ / timer_.GetElapsedSeconds()) / 1000.0;
    }
    if (!app_->has_input_focus()) {
      // Pause the scenario if user alt-tabs etc.
      return PauseAndReturn();
    }
    if (save_crosshair) {
      DoneAdjustingCrosshairSize();
      save_crosshair = false;
    }
    if (show_settings.has_value()) {
      // Need to pause.
      timer_.PauseRun();
      OnPause();
      NavigationEvent nav_event;
      nav_event = CreateQuickSettingsScreen(
                      def_.scenario_id(), *show_settings, show_settings_release_key, app_)
                      ->Run();
      if (nav_event.IsNotDone()) {
        return nav_event;
      }
      show_settings = {};
      // Need to refresh everything based on settings change
      RefreshState();
      timer_.ResumeRun();
    }

    timer_.OnStartFrame();
    current_times_.frame_number = loop_count;
    current_times_.start = timer_.GetElapsedMicros();

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

    current_times_.events_start = timer_.GetElapsedMicros();
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
      if (is_adjusting_crosshair) {
        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
          if (event.wheel.y != 0) {
            crosshair_size_ += event.wheel.y;
          }
        }
      }
      if (IsMappableKeyDownEvent(event)) {
        std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
        if (!ShouldAutoHold()) {
          if (KeyMappingMatchesEvent(event_name, settings_.keybinds().fire())) {
            update_data.has_click = true;
            is_click_held_ = true;
          }
        }
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().restart_scenario())) {
          return NavigationEvent::RestartLastScenario();
        }
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().quick_settings())) {
          show_settings = QuickSettingsType::DEFAULT;
          show_settings_release_key = event_name;
        }
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().quick_metronome())) {
          show_settings = QuickSettingsType::METRONOME;
          show_settings_release_key = event_name;
        }
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().adjust_crosshair_size())) {
          is_adjusting_crosshair = true;
        }
      }
      if (IsEscapeKeyDown(event)) {
        return PauseAndReturn();
      }
      if (IsMappableKeyUpEvent(event)) {
        std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
        if (KeyMappingMatchesEvent(event_name, settings_.keybinds().adjust_crosshair_size())) {
          is_adjusting_crosshair = false;
          save_crosshair = true;
        }
        if (!ShouldAutoHold()) {
          if (KeyMappingMatchesEvent(event_name, settings_.keybinds().fire())) {
            update_data.has_click_up = true;
            is_click_held_ = false;
          }
        }
      }
      OnEvent(event);
    }
    current_times_.events_end = timer_.GetElapsedMicros();

    // Update state

    current_times_.update_start = timer_.GetElapsedMicros();
    if (metronome_) {
      metronome_->DoTick(timer_.GetElapsedMicros());
    }
    look_at_ = camera_.GetLookAt();

    update_data.is_click_held = is_click_held_;
    for (auto& task : delayed_tasks_) {
      if (task.fn.has_value() && task.run_time_seconds < timer_.GetElapsedSeconds()) {
        std::function<void()> fn = std::move(*task.fn);
        task.fn = {};
        fn();
      }
    }
    UpdateState(&update_data);
    num_state_updates_++;
    current_times_.update_end = timer_.GetElapsedMicros();

    // Render if forced or if the last render was over ~1ms ago.
    bool do_render = update_data.force_render ||
                     timer_.LastFrameRenderStartedMicrosAgo() > max_render_age_micros_;
    if (!do_render) {
      current_times_.render_start = 0;
      current_times_.render_end = 0;
      UpdatePerfStats();
      continue;
    }

    timer_.OnStartRender();
    current_times_.render_start = timer_.GetElapsedMicros();
    auto end_render_guard = ScopeGuard::Create([&] { timer_.OnEndRender(); });

    ImDrawList* draw_list = app_->StartFullscreenImguiFrame();
    DrawCrosshair(crosshair_, crosshair_size_, theme_, app_->screen_info(), draw_list);

    float elapsed_seconds = timer_.GetElapsedSeconds();
    ImGui::Text("%s", def_.scenario_id().c_str());
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::Text("ups: %.1fk", state_updates_per_second_);
    ImGui::Text("cm/360: %.0f", settings_.cm_per_360());
    if (settings_.metronome_bpm() > 0) {
      ImGui::Text("metronome bpm: %.0f", settings_.metronome_bpm());
    }

    ImGui::End();

    RenderContext ctx;
    if (app_->StartRender(&ctx)) {
      app_->renderer()->DrawScenario(projection_,
                                     def_.room(),
                                     theme_,
                                     target_manager_.GetTargets(),
                                     look_at_.transform,
                                     &ctx,
                                     timer_.run_stopwatch(),
                                     &current_times_);
      app_->FinishRender(&ctx);
    }
    current_times_.render_end = timer_.GetElapsedMicros();
    UpdatePerfStats();
  }

  stats_.hit_stopwatch.Stop();
  stats_.shot_stopwatch.Stop();
  OnScenarioDone();
  is_done_ = true;

  ShotType::TypeCase shot_type = GetShotType();
  float score = 0;
  switch (shot_type) {
    case ShotType::kTrackingInvincible: {
      stats_.num_hits = stats_.hit_stopwatch.GetElapsedSeconds();
      stats_.num_shots = stats_.shot_stopwatch.GetElapsedSeconds();
      score = stats_.hit_stopwatch.GetElapsedSeconds();
      break;
    }
    case ShotType::kClickSingle: {
      // Default clicking scoring
      float hit_percent = stats_.num_hits / stats_.num_shots;
      float duration_modifier = 60.0f / def_.duration_seconds();
      float accuracy_penalty = 1.0 - sqrt(hit_percent);
      if (def_.has_accuracy_penalty_modifier()) {
        accuracy_penalty *= def_.accuracy_penalty_modifier();
      }
      score = stats_.num_hits * 10 * (1 - accuracy_penalty) * duration_modifier;
      break;
    }
    case ShotType::kTrackingKill:
    case ShotType::kPoke:
    default:
      score = stats_.num_hits;
      break;
  }

  StatsRow stats_row;
  stats_row.cm_per_360 = settings_.cm_per_360();
  stats_row.num_hits = stats_.num_hits;
  stats_row.num_shots = stats_.num_shots;
  stats_row.score = score;
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

  StatsScreen stats_screen(def_.scenario_id(), stats_row.stats_id, app_, perf_stats_);
  return stats_screen.Run(replay_);
}

ShotType::TypeCase Scenario::GetShotType() {
  ShotType::TypeCase shot_type = def_.shot_type().type_case();
  if (shot_type == ShotType::TYPE_NOT_SET) {
    shot_type = GetDefaultShotType();
  }
  return shot_type;
}

void Scenario::UpdatePerfStats() {
  current_times_.end = timer_.GetElapsedMicros();
  current_times_.total = current_times_.end - current_times_.start;

  perf_stats_.total_time_histogram.Increment(current_times_.total);
  perf_stats_.render_time_histogram.Increment(current_times_.render_end -
                                              current_times_.render_start);
  if (current_times_.total > perf_stats_.worst_times.total) {
    perf_stats_.worst_times = current_times_;
  }
}

void Scenario::DoneAdjustingCrosshairSize() {
  Settings* current_settings = app_->settings_manager()->GetMutableCurrentSettings();
  if (current_settings != nullptr) {
    if (crosshair_size_ != current_settings->crosshair_size()) {
      current_settings->set_crosshair_size(crosshair_size_);
      app_->settings_manager()->MarkDirty();
      app_->settings_manager()->MaybeFlushToDisk(def_.scenario_id());
      RefreshState();
    }
  }
}

void Scenario::AddNewTargetEvent(const Target& target) {
  if (replay_ != nullptr) {
    auto event = replay_->add_events();
    event->set_time_seconds(timer_.GetElapsedSeconds());
    auto add_target = event->mutable_add_target();
    add_target->set_target_id(target.id);
    *(add_target->mutable_position()) = ToStoredVec3(target.position);
    add_target->set_radius(target.radius);
  }
}

void Scenario::AddKillTargetEvent(u16 target_id) {
  if (!replay_) {
    return;
  }
  auto event = replay_->add_events();
  event->set_time_seconds(timer_.GetElapsedSeconds());
  event->mutable_kill_target()->set_target_id(target_id);
}

void Scenario::AddRemoveTargetEvent(u16 target_id) {
  if (!replay_) {
    return;
  }
  auto event = replay_->add_events();
  event->set_time_seconds(timer_.GetElapsedSeconds());
  event->mutable_remove_target()->set_target_id(target_id);
}

void Scenario::AddShotFiredEvent() {
  if (!replay_) {
    return;
  }
  auto event = replay_->add_events();
  event->set_time_seconds(timer_.GetElapsedSeconds());
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
                                        float distance_per_second) {}

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
  target.health_seconds = GetJitteredValue(
      profile.health_seconds(), profile.health_seconds_jitter(), app_->random_generator());
  if (profile.has_pill()) {
    target.is_pill = true;
    target.height = profile.pill().height();
    if (profile.pill().has_up()) {
      target.pill_up = ToVec3(profile.pill().up());
    }
    if (profile.pill().has_wall_up()) {
      target.pill_wall_up = ToVec2(profile.pill().wall_up());
    }
  }
  return target;
}

NavigationEvent Scenario::PauseAndReturn() {
  timer_.PauseRun();
  OnPause();
  return NavigationEvent::Done();
}

std::unique_ptr<Scenario> CreateScenario(const ScenarioDef& def, Application* app) {
  switch (def.type_case()) {
    case ScenarioDef::kStaticDef:
      return CreateStaticScenario(def, app);
    case ScenarioDef::kCenteringDef:
      return CreateCenteringScenario(def, app);
    case ScenarioDef::kBarrelDef:
      return CreateBarrelScenario(def, app);
    case ScenarioDef::kLinearDef:
      return CreateLinearScenario(def, app);
    case ScenarioDef::kWallStrafeDef:
      return CreateWallStrafeScenario(def, app);
    default:
      break;
  }
  return {};
}

}  // namespace aim

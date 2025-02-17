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
#include "aim/scenario/stats_screen.h"
#include "aim/ui/settings_screen.h"

namespace aim {
namespace {
constexpr const u16 kReplayFps = 240;

std::unique_ptr<Scenario> CreateScenarioForType(const ScenarioDef& def, Application* app) {
  switch (def.type_case()) {
    case ScenarioDef::kStaticDef:
      return CreateStaticScenario(def, app);
    case ScenarioDef::kCenteringDef:
      return CreateCenteringScenario(def, app);
    default:
      break;
  }
  return {};
}

}  // namespace

Scenario::Scenario(const ScenarioDef& def, Application* app)
    : def_(def),
      app_(app),
      metronome_(0, app),
      timer_(kReplayFps),
      camera_(Camera(
          def.room().start_pitch(), def.room().start_yaw(), ToVec3(def.room().camera_position()))),
      replay_(std::make_unique<Replay>()) {
  theme_ = app->GetSettingsManager()->GetCurrentTheme();
}

NavigationEvent Scenario::Run() {
  ScreenInfo screen = app_->GetScreenInfo();
  glm::mat4 projection = GetPerspectiveTransformation(screen);
  float dpi = app_->GetSettingsManager()->GetDpi();
  Settings settings = app_->GetSettingsManager()->GetCurrentSettings();
  float radians_per_dot = CmPer360ToRadiansPerDot(settings.cm_per_360(), dpi);

  *replay_->mutable_room() = def_.room();
  replay_->set_replay_fps(timer_.GetReplayFps());
  Initialize();

  SDL_GL_SetSwapInterval(0);  // Disable vsync
  SDL_SetWindowRelativeMouseMode(app_->GetSdlWindow(), true);
  app_->GetRenderer()->SetProjection(projection);

  // Main loop
  bool stop_scenario = false;
  bool show_settings = false;
  bool is_quick_settings = true;
  bool is_click_held = false;
  u64 num_state_updates = 0;
  while (!stop_scenario) {
    if (!app_->has_input_focus()) {
      // Pause the scenario if user alt-tabs etc.
      show_settings = true;
      is_quick_settings = false;
    }
    if (show_settings) {
      // Need to pause.
      timer_.Pause();
      OnPause();
      NavigationEvent nav_event;
      if (is_quick_settings) {
        QuickSettingsScreen settings_screen;
        nav_event = settings_screen.Run(app_);
      } else {
        SettingsScreen settings_screen;
        nav_event = settings_screen.Run(app_);
        app_->logger()->flush();
      }
      if (nav_event.IsNotDone()) {
        return nav_event;
      }
      show_settings = false;
      is_quick_settings = false;

      // Need to refresh everything based on settings change
      settings = app_->GetSettingsManager()->GetCurrentSettings();
      radians_per_dot = CmPer360ToRadiansPerDot(settings.cm_per_360(), dpi);
      theme_ = app_->GetSettingsManager()->GetCurrentTheme();
      is_click_held = false;
      timer_.Resume();
      SDL_GL_SetSwapInterval(0);  // Disable vsync
      SDL_SetWindowRelativeMouseMode(app_->GetSdlWindow(), true);
      OnResume();
    }

    timer_.OnStartFrame();

    if (timer_.IsNewReplayFrame()) {
      // Store the look at vector before the mouse updates for the old frame.
      replay_->add_pitch_yaws(camera_.GetPitch());
      replay_->add_pitch_yaws(camera_.GetYaw());
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
        return NavigationEvent::Exit();
      }
      if (event.type == SDL_EVENT_MOUSE_MOTION) {
        camera_.Update(event.motion.xrel, event.motion.yrel, radians_per_dot);
      }
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          update_data.has_click = true;
          is_click_held = true;
        }
      }
      if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          update_data.has_click_up = true;
          is_click_held = false;
        }
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_R) {
          return NavigationEvent::RestartLastScenario();
        }
        if (keycode == SDLK_S) {
          show_settings = true;
          is_quick_settings = true;
        }
        if (keycode == SDLK_ESCAPE) {
          show_settings = true;
          is_quick_settings = false;
        }
      }
      OnEvent(event);
    }

    // Update state

    metronome_.DoTick(timer_.GetElapsedMicros());
    look_at_ = camera_.GetLookAt();

    update_data.is_click_held = is_click_held;
    UpdateState(&update_data);
    num_state_updates++;

    // Render if forced or if the last render was over ~1ms ago.
    bool do_render = update_data.force_render || timer_.LastFrameRenderedMicrosAgo() > 1200;
    if (!do_render) {
      continue;
    }

    timer_.OnStartRender();
    auto end_render_guard = ScopeGuard::Create([&] { timer_.OnEndRender(); });

    ImDrawList* draw_list = app_->StartFullscreenImguiFrame();
    DrawCrosshair(settings.crosshair(), theme_, screen, draw_list);

    float elapsed_seconds = timer_.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::Text("cm/360: %.0f", settings.cm_per_360());

    // ~ Around 450k
    // ImGui::Text("ups: %.1f", num_state_updates / elapsed_seconds);

    ImGui::End();

    if (app_->StartRender(ImVec4(0, 0, 0, 1))) {
      app_->GetRenderer()->DrawScenario(
          def_.room(), theme_, target_manager_.GetTargets(), look_at_.transform);
      app_->FinishRender();
    }
  }

  OnScenarioDone();

  if (stats_.shots_taken == 0) {
    return NavigationEvent::GoHome();
  }

  stats_.hit_percent = stats_.targets_hit / (float)stats_.shots_taken;
  float duration_modifier = 60.0f / def_.duration_seconds();
  stats_.score = stats_.targets_hit * 10 * sqrt(stats_.hit_percent) * duration_modifier;

  StatsRow stats_row;
  stats_row.cm_per_360 = settings.cm_per_360();
  stats_row.num_hits = stats_.targets_hit;
  stats_row.num_kills = stats_.targets_hit;
  stats_row.num_shots = stats_.shots_taken;
  stats_row.score = stats_.score;
  app_->GetStatsDb()->AddStats(def_.scenario_id(), &stats_row);

  StatsScreen stats_screen(
      def_.scenario_id(), stats_row.stats_id, std::move(replay_), stats_, app_);
  return stats_screen.Run();
}

NavigationEvent RunScenario(const ScenarioDef& def, Application* app) {
  while (true) {
    std::unique_ptr<Scenario> scenario = CreateScenarioForType(def, app);
    if (!scenario) {
      app->logger()->warn("Unable to make scenario for def: {}", def.DebugString());
      return NavigationEvent::Done();
    }
    NavigationEvent nav_event = scenario->Run();
    if (nav_event.type != NavigationEventType::RESTART_LAST_SCENARIO) {
      app->logger()->flush();
      return nav_event;
    }
  }
}

}  // namespace aim

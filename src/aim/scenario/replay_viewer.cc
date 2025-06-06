#include "replay_viewer.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>

#include <algorithm>

#include "aim/common/scope_guard.h"
#include "aim/graphics/crosshair.h"
#include "aim/scenario/scenario_timer.h"

namespace aim {
namespace {

bool CompareEventsByTime(const ReplayEvent& lhs, const ReplayEvent& rhs) {
  return lhs.time_seconds() < rhs.time_seconds();
}

}  // namespace

void ReplayViewer::PlayReplay(const Replay& replay, Application* app) {
  Theme theme = app->settings_manager().GetCurrentTheme();
  Settings settings = app->settings_manager().GetCurrentSettings();
  Crosshair crosshair = app->settings_manager().GetCurrentCrosshair();
  float crosshair_size = settings.crosshair_size();

  ScreenInfo screen = app->screen_info();
  glm::mat4 projection = GetPerspectiveTransformation(screen);

  std::vector<ReplayEvent> events(replay.events().begin(), replay.events().end());
  std::sort(events.begin(), events.end(), CompareEventsByTime);

  int processed_events_up_to_index = 0;

  TargetManager target_manager(replay.room());
  Camera camera(CameraParams(replay.room()));

  FrameTimes times;
  ScenarioTimer timer(replay.replay_fps());
  timer.StartLoop();
  timer.ResumeRun();
  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        throw ApplicationExitException();
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_ESCAPE) {
          //    return NavigationEvent::Done();
          return;
        }
      }
    }
    timer.OnStartFrame();

    uint64_t replay_frame_number = timer.GetReplayFrameNumber();
    int pitch_yaws_index = replay_frame_number * 2;
    if (pitch_yaws_index + 1 >= replay.pitch_yaws_size()) {
      // return NavigationEvent::Done();
      return;
    }

    camera.UpdatePitch(replay.pitch_yaws(pitch_yaws_index));
    camera.UpdateYaw(replay.pitch_yaws(pitch_yaws_index + 1));
    LookAtInfo look_at = camera.GetLookAt();

    bool force_render = false;
    bool has_kill = false;
    bool has_shot = false;
    float now_seconds = timer.GetElapsedSeconds();
    for (int i = processed_events_up_to_index; i < events.size(); ++i) {
      ReplayEvent& event = events[i];
      if (event.time_seconds() > now_seconds) {
        break;
      }

      // Process the event.
      if (event.has_kill_target()) {
        target_manager.RemoveTarget(event.kill_target().target_id());
        has_kill = true;
        force_render = true;
      }
      if (event.has_remove_target()) {
        target_manager.RemoveTarget(event.remove_target().target_id());
        force_render = true;
      }
      if (event.has_shot_fired()) {
        has_shot = true;
      }
      if (event.has_add_target()) {
        Target t;
        t.id = event.add_target().target_id();
        t.radius = event.add_target().radius();
        t.position = ToVec3(event.add_target().position());
        target_manager.AddTarget(t);
        force_render = true;
      }
      processed_events_up_to_index = i + 1;
    }

    if (has_shot) {
      app->sound_manager()->PlayShootSound(settings.sound().shoot());
    }
    if (has_kill) {
      app->sound_manager()->PlayKillSound(settings.sound().kill());
    }

    bool do_render = force_render || timer.LastFrameRenderStartedMicrosAgo() > 2500;
    if (!do_render) {
      continue;
    }

    timer.OnStartRender();
    auto end_render_guard = ScopeGuard::Create([&] { timer.OnEndRender(); });

    app->NewImGuiFrame();
    app->BeginFullscreenWindow();
    DrawCrosshair(crosshair, crosshair_size, theme, screen.center);

    float elapsed_seconds = timer.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    RenderContext ctx;
    if (app->StartRender(&ctx)) {
      app->renderer()->DrawScenario(projection,
                                    replay.room(),
                                    theme,
                                    settings.health_bar(),
                                    target_manager.GetTargets(),
                                    look_at,
                                    &ctx,
                                    timer.run_stopwatch(),
                                    &times);
      app->FinishRender(&ctx);
    }
  }
}

}  // namespace aim

#include "replay_viewer.h"

#include <algorithm>

#include "SDL3/SDL.h"
#include "absl/cleanup/cleanup.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"
#include "aim/graphics/renderer.h"
#include "aim/scenario/scenario_timer.h"
#include "imgui/backends/imgui_impl_sdl3.h"

namespace aim {
namespace {

void PlaySound(SoundManager* sound_manager, const SoundSettings& settings, ReplaySoundType type) {
  switch (type) {
    case ReplaySoundType::KILL:
      sound_manager->PlayKillSound(settings.kill());
      break;
    case ReplaySoundType::HIT:
      sound_manager->PlayHitSound(settings.hit());
      break;
    case ReplaySoundType::SHOOT:
      sound_manager->PlayShootSound(settings.shoot());
      break;
  }
}

}  // namespace

void ReplayViewer::PlayReplay(const ReplayV2& replay, Application* app) {
  float approximate_mb = replay.GetApproximateSizeMb();
  Theme theme = app->settings_manager().GetCurrentTheme();
  Settings settings = app->settings_manager().GetCurrentSettings();
  Crosshair crosshair = app->settings_manager().GetCurrentCrosshair();
  float crosshair_size = settings.crosshair_size();

  ScreenInfo screen = app->screen_info();
  glm::mat4 projection = GetPerspectiveTransformation(screen, replay.room.horizontal_fov());

  const std::vector<ReplayEvent>& events = replay.events;

  int processed_events_up_to_index = 0;
  int processed_targets_up_to_index = 0;

  std::unordered_map<u16, u16> target_data_channel_map;

  TargetManager target_manager(replay.room);
  Camera camera(CameraParams(replay.room));

  FrameTimes times;
  ScenarioTimer timer(replay.replay_fps);
  timer.StartLoop();
  timer.ResumeRun();
  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        app->RequestExit();
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

    bool force_render = false;
    if (timer.IsNewReplayFrame()) {
      force_render = true;
      i64 replay_frame_number = timer.GetReplayFrameNumber();
      if (replay_frame_number >= replay.pitch_yaws.size()) {
        // return NavigationEvent::Done();
        return;
      }

      const PitchYaw& pitch_yaw = replay.pitch_yaws[replay_frame_number];
      camera.UpdatePitchYaw(pitch_yaw);

      {
        i64 start_index = replay_frame_number * replay.num_targets;
        int end_index = start_index + replay.num_targets;
        for (auto& entry : target_data_channel_map) {
          u16 data_channel = entry.second;
          u16 target_id = entry.first;

          Target* target = target_manager.GetMutableTarget(target_id);
          if (target != nullptr) {
            i64 i = start_index + data_channel;
            if (IsValidIndex(replay.target_data, i)) {
              const TargetData& data = replay.target_data[i];
              target->position = data.position;
              target->radius = data.radius;
            }
          }
        }
      }
    }

    LookAtInfo look_at = camera.GetLookAt();
    float now_seconds = timer.GetElapsedSeconds();

    for (int i = processed_events_up_to_index; i < events.size(); ++i) {
      const ReplayEvent& event = events[i];
      if (event.time_seconds > now_seconds) {
        break;
      }

      // Process the event.
      switch (event.type) {
        case ReplayEventType::REMOVE_TARGET:
          target_manager.RemoveTarget(event.data.remove_target.target_id);
          target_data_channel_map.erase(event.data.remove_target.target_id);
          force_render = true;
          break;
        case ReplayEventType::PLAY_SOUND:
          PlaySound(app->sound_manager(), settings.sound(), event.data.play_sound.sound);
          break;
      }
      processed_events_up_to_index = i + 1;
    }

    for (int i = processed_targets_up_to_index; i < replay.target_metadata.size(); ++i) {
      const ReplayTargetMetadata& metadata = replay.target_metadata[i];
      if (metadata.add_time_seconds > now_seconds) {
        break;
      }

      Target t;
      t.id = metadata.target_id;
      t.radius = metadata.initial_data.radius;
      t.position = metadata.initial_data.position;
      if (metadata.pill_height > 0) {
        t.is_pill = true;
        t.height = metadata.pill_height;
      }
      target_data_channel_map[t.id] = metadata.data_channel;
      target_manager.AddTarget(t);
      force_render = true;
      processed_targets_up_to_index = i + 1;
    }

    bool do_render = force_render || timer.LastFrameRenderStartedMicrosAgo() > 2500;
    if (!do_render) {
      continue;
    }

    timer.OnStartRender();
    auto end_render_guard = absl::MakeCleanup([&] { timer.OnEndRender(); });

    app->NewImGuiFrame();
    app->BeginFullscreenWindow();
    app->crosshair_manager().Draw(crosshair, crosshair_size, theme, screen.center);

    float elapsed_seconds = timer.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    RenderContext ctx;
    if (app->StartRender(&ctx)) {
      app->renderer()->DrawScenario(projection,
                                    replay.room,
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

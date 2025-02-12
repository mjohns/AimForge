#include "replay_viewer.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>

#include "aim/common/scope_guard.h"
#include "aim/graphics/crosshair.h"
#include "aim/scenario/scenario_timer.h"

namespace aim {
namespace {

struct ReplayFrame {
  std::vector<ReplayEvent> events;
  float pitch;
  float yaw;
};

// Collect all info necessary to render one frame into a single struct.
std::vector<ReplayFrame> GetReplayFrames(const Replay& replay) {
  std::vector<ReplayFrame> replay_frames;
  for (int frame_number = 0; frame_number < replay.pitch_yaws_size() / 2; ++frame_number) {
    int i = frame_number * 2;
    ReplayFrame frame;
    frame.pitch = replay.pitch_yaws(i);
    frame.yaw = replay.pitch_yaws(i + 1);
    for (auto& event : replay.events()) {
      if (event.frame_number() == frame_number) {
        frame.events.push_back(event);
      }
    }
    replay_frames.push_back(frame);
  }
  return replay_frames;
}

}  // namespace

NavigationEvent ReplayViewer::PlayReplay(const Replay& replay,
                                         const Crosshair& crosshair,
                                         Application* app) {
  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = GetPerspectiveTransformation(screen);

  glm::vec3 camera_position = ToVec3(replay.camera_position());

  std::vector<ReplayFrame> replay_frames = GetReplayFrames(replay);

  app->GetRenderer()->SetProjection(projection);

  TargetManager target_manager;
  Camera camera(camera_position);

  ScenarioTimer timer(replay.replay_fps());
  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return NavigationEvent::Exit();
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_ESCAPE) {
          return NavigationEvent::Done();
        }
      }
    }
    timer.OnStartFrame();

    uint64_t replay_frame_number = timer.GetReplayFrameNumber();
    if (replay_frame_number >= replay_frames.size()) {
      return NavigationEvent::Done();
    }

    if (!timer.IsNewReplayFrame()) {
      continue;
    }

    timer.OnStartRender();
    auto end_render_guard = ScopeGuard::Create([&] { timer.OnEndRender(); });

    auto& replay_frame = replay_frames[replay_frame_number];
    camera.UpdatePitch(replay_frame.pitch);
    camera.UpdateYaw(replay_frame.yaw);
    LookAtInfo look_at = camera.GetLookAt();

    bool has_kill = false;
    bool has_miss = false;
    for (auto& event : replay_frame.events) {
      if (event.has_kill_target()) {
        target_manager.RemoveTarget(event.kill_target().target_id());
        has_kill = true;
      }
      if (event.has_remove_target()) {
        target_manager.RemoveTarget(event.remove_target().target_id());
      }
      if (event.has_shot_missed()) {
        has_miss = true;
      }
      if (event.has_add_static_target()) {
        Target t;
        t.id = event.add_static_target().target_id();
        t.radius = event.add_static_target().radius();
        t.position = ToVec3(event.add_static_target().position());
        target_manager.AddTarget(t);
      }
    }

    if (has_kill) {
      if (replay.is_poke_ball()) {
        app->GetSoundManager()->PlayKillSound();
      } else {
        app->GetSoundManager()->PlayKillSound().PlayShootSound();
      }
    } else if (has_miss) {
      app->GetSoundManager()->PlayShootSound();
    }

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();

    DrawCrosshair(crosshair, screen, draw_list);

    float elapsed_seconds = timer.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    if (app->StartRender()) {
      app->GetRenderer()->DrawScenario(
          replay.room(), target_manager.GetTargets(), look_at.transform);
      app->FinishRender();
    }
  }
  return NavigationEvent::Done();
}

}  // namespace aim

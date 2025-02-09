#include "replay_viewer.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>

#include "aim/common/scope_guard.h"
#include "aim/graphics/crosshair.h"
#include "aim/scenario/scenario_timer.h"

namespace aim {
namespace {

struct ReplayFrame {
  std::vector<AddTargetEventT*> add_target_events;
  std::vector<HitTargetEventT*> hit_target_events;
  std::vector<MissTargetEventT*> miss_target_events;
  std::vector<RemoveTargetEventT*> remove_target_events;
  PitchYaw pitch_yaw;
};

// Collect all info necessary to render one frame into a single struct.
std::vector<ReplayFrame> GetReplayFrames(const StaticReplayT& replay) {
  std::vector<ReplayFrame> replay_frames;
  for (int frame_number = 0; frame_number < replay.pitch_yaw_pairs.size(); ++frame_number) {
    ReplayFrame frame;
    frame.pitch_yaw = replay.pitch_yaw_pairs[frame_number];
    for (auto& event : replay.hit_target_events) {
      if (event->frame_number == frame_number) {
        frame.hit_target_events.push_back(event.get());
      }
    }
    for (auto& event : replay.add_target_events) {
      if (event->frame_number == frame_number) {
        frame.add_target_events.push_back(event.get());
      }
    }
    for (auto& event : replay.miss_target_events) {
      if (event->frame_number == frame_number) {
        frame.miss_target_events.push_back(event.get());
      }
    }
    for (auto& event : replay.remove_target_events) {
      if (event->frame_number == frame_number) {
        frame.remove_target_events.push_back(event.get());
      }
    }
    replay_frames.push_back(frame);
  }
  return replay_frames;
}

}  // namespace

NavigationEvent ReplayViewer::PlayReplay(const StaticReplayT& replay,
                                         const CrosshairT& crosshair,
                                         Application* app) {
  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = GetPerspectiveTransformation(screen);

  auto stored_camera_position = replay.camera_position.get();
  glm::vec3 camera_position = ToVec3(*stored_camera_position);

  std::vector<ReplayFrame> replay_frames = GetReplayFrames(replay);

  app->GetRenderer()->SetProjection(projection);

  TargetManager target_manager;
  Camera camera(camera_position);

  ScenarioTimer timer(replay.frames_per_second);
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
          return NavigationEvent::GoBack();
        }
      }
    }
    timer.OnStartFrame();

    uint64_t replay_frame_number = timer.GetReplayFrameNumber();
    if (replay_frame_number >= replay_frames.size()) {
      return NavigationEvent::GoBack();
    }

    if (!timer.IsNewReplayFrame()) {
      continue;
    }

    timer.OnStartRender();
    auto end_render_guard = ScopeGuard::Create([&] { timer.OnEndRender(); });

    auto& replay_frame = replay_frames[replay_frame_number];
    camera.UpdatePitch(replay_frame.pitch_yaw.pitch());
    camera.UpdateYaw(replay_frame.pitch_yaw.yaw());
    LookAtInfo look_at = camera.GetLookAt();

    bool has_hit = replay_frame.hit_target_events.size() > 0;
    bool has_miss = replay_frame.miss_target_events.size() > 0;
    if (has_hit) {
      if (replay.is_poke_ball) {
        app->GetSoundManager()->PlayKillSound();
      } else {
        app->GetSoundManager()->PlayKillSound().PlayShootSound();
      }
    } else if (has_miss) {
      app->GetSoundManager()->PlayShootSound();
    }
    // Play miss sound.
    for (HitTargetEventT* event : replay_frame.hit_target_events) {
      target_manager.RemoveTarget(event->target_id);
    }
    for (RemoveTargetEventT* event : replay_frame.remove_target_events) {
      target_manager.RemoveTarget(event->target_id);
    }
    for (AddTargetEventT* event : replay_frame.add_target_events) {
      Target t;
      t.id = event->target_id;
      t.radius = event->radius;
      t.position = ToVec3(*event->position);
      target_manager.AddTarget(t);
    }

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();

    DrawCrosshair(crosshair, screen, draw_list);

    float elapsed_seconds = timer.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    if (app->StartRender()) {
      app->GetRenderer()->DrawSimpleStaticRoom(
          replay.wall_height, replay.wall_width, target_manager.GetTargets(), look_at.transform);
      app->FinishRender();
    }
  }
  return NavigationEvent::GoBack();
}

}  // namespace aim

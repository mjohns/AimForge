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

struct MovementInfo {
  u16 target_id = 0;
  glm::vec3 direction;
  float speed;
  float last_update_time_seconds;
};

}  // namespace

NavigationEvent ReplayViewer::PlayReplay(const Replay& replay, Application* app) {
  Theme theme = app->settings_manager()->GetCurrentTheme();
  Crosshair crosshair = app->settings_manager()->GetCurrentCrosshair();
  float crosshair_size = app->settings_manager()->GetCurrentSettings().crosshair_size();

  ScreenInfo screen = app->screen_info();
  glm::mat4 projection = GetPerspectiveTransformation(screen);

  std::vector<ReplayFrame> replay_frames = GetReplayFrames(replay);

  std::unordered_map<u16, MovementInfo> move_target_map;

  app->renderer()->SetProjection(projection);

  TargetManager target_manager(replay.room());
  Camera camera(replay.room().start_pitch(),
                replay.room().start_yaw(),
                ToVec3(replay.room().camera_position()));

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
    bool has_shot = false;
    float now_seconds = timer.GetElapsedSeconds();
    for (auto& event : replay_frame.events) {
      if (event.has_kill_target()) {
        target_manager.RemoveTarget(event.kill_target().target_id());
        has_kill = true;
      }
      if (event.has_remove_target()) {
        target_manager.RemoveTarget(event.remove_target().target_id());
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
      }
      if (event.has_move_linear_target()) {
        auto m = event.move_linear_target();
        MovementInfo info;
        info.target_id = m.target_id();
        info.direction = ToVec3(m.direction());
        info.speed = m.distance_per_second();
        info.last_update_time_seconds = now_seconds;
        move_target_map[event.move_linear_target().target_id()] = info;

        Target* t = target_manager.GetMutableTarget(m.target_id());
        if (t != nullptr) {
          t->position = ToVec3(m.starting_position());
        }
      }
    }

    if (has_shot) {
      app->sound_manager()->PlayShootSound();
    }
    if (has_kill) {
      app->sound_manager()->PlayKillSound();
    }

    // Move targets.
    for (Target* t : target_manager.GetMutableVisibleTargets()) {
      if (move_target_map.find(t->id) != move_target_map.end()) {
        MovementInfo& info = move_target_map[t->id];
        float delta_seconds = now_seconds - info.last_update_time_seconds;
        t->position = t->position + (info.direction * (delta_seconds * info.speed));
        info.last_update_time_seconds = now_seconds;
      }
    }

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();

    DrawCrosshair(crosshair, crosshair_size, theme, screen, draw_list);

    float elapsed_seconds = timer.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    if (app->StartRender(ImVec4(0, 0, 0, 1))) {
      app->renderer()->DrawScenario(
          replay.room(), theme, target_manager.GetTargets(), look_at.transform);
      app->FinishRender();
    }
  }
  return NavigationEvent::Done();
}

}  // namespace aim

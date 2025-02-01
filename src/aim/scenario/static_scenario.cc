#include "static_scenario.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <flatbuffers/flatbuffers.h>
#include <imgui.h>

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
#include "aim/fbs/common_generated.h"
#include "aim/fbs/replay_generated.h"
#include "aim/fbs/settings_generated.h"
#include "aim/graphics/crosshair.h"
#include "aim/graphics/room.h"
#include "aim/graphics/shader.h"
#include "aim/graphics/sphere.h"
#include "aim/scenario/scenario_timer.h"

namespace aim {
namespace {

Camera GetInitialCamera(const StaticScenarioParams& params) {
  return Camera(glm::vec3(0, -100.0f, 0));
}

CrosshairT GetDefaultCrosshair() {
  auto dot = std::make_unique<DotCrosshairT>();
  dot->dot_color = ToStoredRgbPtr(254, 138, 24);
  dot->outline_color = ToStoredRgbPtr(0, 0, 0);
  dot->draw_outline = true;
  dot->dot_size = 3;

  CrosshairT crosshair;
  crosshair.dot = std::move(dot);
  return crosshair;
}

struct ReplayFrame {
  std::vector<AddTargetEventT*> add_target_events;
  std::vector<HitTargetEventT*> hit_target_events;
  std::vector<MissTargetEventT*> miss_target_events;
  PitchYaw pitch_yaw;
};

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
    replay_frames.push_back(frame);
  }
  return replay_frames;
}

void DrawFrame(Application* app,
               TargetManager* target_manager,
               SphereRenderer* sphere_renderer,
               Room* room,
               const glm::mat4& view_transform) {
  ImVec4 clear_color = ImVec4(0.7f, 0.7f, 0.7f, 1.00f);
  if (app->StartRender(clear_color)) {
    room->Draw(view_transform);
    std::vector<Sphere> target_spheres;
    for (const Target& target : target_manager->GetTargets()) {
      if (!target.hidden) {
        sphere_renderer->Draw(view_transform, {{target.position, target.radius}});
      }
    }
    app->FinishRender();
  }
}

void PlayReplay(const StaticReplayT& replay, Application* app, const std::string& score_string) {
  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = GetPerspectiveTransformation(screen);

  auto stored_camera_position = replay.camera_position.get();
  glm::vec3 camera_position = ToVec3(*stored_camera_position);

  Sounds sounds = GetDefaultSounds();

  std::vector<ReplayFrame> replay_frames = GetReplayFrames(replay);

  RoomParams room_params;
  room_params.wall_height = replay.wall_height;
  room_params.wall_width = replay.wall_width;
  Room room(room_params);
  room.SetProjection(projection);

  SphereRenderer sphere_renderer;
  sphere_renderer.SetProjection(projection);

  TargetManager target_manager;
  Camera camera(camera_position);

  auto crosshair = GetDefaultCrosshair();

  ScenarioTimer timer(replay.frames_per_second);
  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return;
      }
    }
    timer.OnStartFrame();

    uint64_t replay_frame_number = timer.GetReplayFrameNumber();
    if (replay_frame_number >= replay_frames.size()) {
      return;
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
      if (sounds.shoot) {
        sounds.shoot->Play();
      }
      if (sounds.kill) {
        sounds.kill->Play();
      }
    } else if (has_miss) {
      if (sounds.shoot) {
        sounds.shoot->Play();
      }
    }
    // Play miss sound.
    for (HitTargetEventT* event : replay_frame.hit_target_events) {
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
    ImGui::Text("score: %s", score_string.c_str());
    ImGui::End();

    DrawFrame(app, &target_manager, &sphere_renderer, &room, look_at.transform);
  }
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

glm::vec2 GetRandomPositionInCircle(float max_radius, Application* app) {
  auto dist_radius = std::uniform_real_distribution<float>(0, max_radius);
  auto dist_degrees = std::uniform_real_distribution<float>(0.01, 360);

  float radius = dist_radius(*app->GetRandomGenerator());
  float rotate_radians = glm::radians(dist_degrees(*app->GetRandomGenerator()));

  double cos_angle = cos(rotate_radians);
  double sin_angle = sin(rotate_radians);

  float x = -1 * radius * sin_angle;
  float y = radius * cos_angle;
  return glm::vec2(x, y);
}

// Returns an x/z pair where to place the target on the back wall.
glm::vec2 GetNewTargetPosition(const RoomParams& room_params,
                               TargetManager* target_manager,
                               Application* app) {
  // Allow placing within the middle x% of the specified dimension
  float x_percent = 0.7;
  float y_percent = 0.6;

  float max_x = 0.5 * room_params.wall_width * x_percent;
  float max_y = 0.5 * room_params.wall_height * y_percent;

  auto distribution_x = std::uniform_real_distribution<float>(-1 * max_x, max_x);
  auto distribution_y = std::uniform_real_distribution<float>(-1 * max_y, max_y);

  auto dist_circle = std::uniform_real_distribution<float>(0, 1);

  float min_distance = 20;

  int max_attempts = 30;
  int attempt_number = 0;
  while (true) {
    ++attempt_number;

    glm::vec2 pos;

    // Place the target within the smaller center circle area with a higher probability.
    float use_circle_roll = dist_circle(*app->GetRandomGenerator());
    if (use_circle_roll < 0.7) {
      pos = GetRandomPositionInCircle(max_y, app);
    } else {
      pos.x = distribution_x(*app->GetRandomGenerator());
      pos.y = distribution_y(*app->GetRandomGenerator());
    }

    if (attempt_number >= max_attempts ||
        AreNoneWithinDistance(pos, min_distance, target_manager)) {
      return pos;
    }
  }
}

Target GetNewTarget(const RoomParams& room_params,
                    const StaticScenarioParams& params,
                    TargetManager* target_manager,
                    Application* app) {
  glm::vec2 pos = GetNewTargetPosition(room_params, target_manager, app);
  Target t;
  t.position.x = pos.x;
  t.position.z = pos.y;
  // Make sure the target does not clip throug wall
  t.position.y = -1 * (params.target_radius + 0.5);
  t.radius = params.target_radius;
  return t;
}

}  // namespace

StaticScenario::StaticScenario(StaticScenarioParams params)
    : _camera(GetInitialCamera(params)), _params(params) {}

void StaticScenario::Run(Application* app) {
  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = GetPerspectiveTransformation(screen);
  Sounds sounds = GetDefaultSounds();
  float radians_per_dot = CmPer360ToRadiansPerDot(45, 1600);
  auto crosshair = GetDefaultCrosshair();

  RoomParams room_params;
  room_params.wall_height = _params.room_height;
  room_params.wall_width = _params.room_width;

  StaticReplayT replay;
  uint16_t replay_frames_per_second = 100;
  replay.frames_per_second = replay_frames_per_second;
  replay.camera_position = ToStoredVec3Ptr(_camera.GetPosition());

  for (int i = 0; i < _params.num_targets; ++i) {
    Target target =
        _target_manager.AddTarget(GetNewTarget(room_params, _params, &_target_manager, app));

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
  SDL_SetWindowRelativeMouseMode(app->GetSdlWindow(), true);

  // Room
  Room room(room_params);
  room.SetProjection(projection);
  replay.wall_width = room.GetWidth();
  replay.wall_height = room.GetHeight();

  SphereRenderer sphere_renderer;
  sphere_renderer.SetProjection(projection);

  RunStats stats;

  // Main loop
  ScenarioTimer timer(replay_frames_per_second);
  bool stop_scenario = false;
  while (!stop_scenario) {
    timer.OnStartFrame();

    if (timer.IsNewReplayFrame()) {
      // Store the look at vector before the mouse updates for the old frame.
      replay.pitch_yaw_pairs.push_back(PitchYaw(_camera.GetPitch(), _camera.GetYaw()));
    }

    if (timer.GetElapsedSeconds() >= _params.duration_seconds) {
      stop_scenario = true;
      continue;
    }

    SDL_Event event;
    bool has_click = false;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return;
      }
      if (event.type == SDL_EVENT_MOUSE_MOTION) {
        _camera.Update(event.motion.xrel, event.motion.yrel, radians_per_dot);
      }
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        has_click = true;
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_S) {
          stop_scenario = true;
        }
      }
    }

    // Update state

    bool force_render = false;
    look_at = _camera.GetLookAt();

    if (has_click) {
      stats.shots_taken++;
      auto maybe_hit_target_id = _target_manager.GetNearestHitTarget(_camera, look_at.front);
      if (maybe_hit_target_id.has_value()) {
        stats.targets_hit++;
        sounds.kill->Play();
        force_render = true;

        auto hit_target_id = *maybe_hit_target_id;
        Target new_target = _target_manager.ReplaceTarget(
            hit_target_id, GetNewTarget(room_params, _params, &_target_manager, app));

        // Add replay events
        auto add_target = std::make_unique<AddTargetEventT>();
        add_target->target_id = new_target.id;
        add_target->frame_number = timer.GetReplayFrameNumber();
        add_target->position = ToStoredVec3Ptr(new_target.position);
        add_target->radius = new_target.radius;
        replay.add_target_events.push_back(std::move(add_target));

        auto hit_target = std::make_unique<HitTargetEventT>();
        hit_target->target_id = hit_target_id;
        hit_target->frame_number = timer.GetReplayFrameNumber();
        replay.hit_target_events.push_back(std::move(hit_target));
      } else {
        // Missed shot
        sounds.shoot->Play();
        auto miss_target = std::make_unique<MissTargetEventT>();
        miss_target->frame_number = timer.GetReplayFrameNumber();
        replay.miss_target_events.push_back(std::move(miss_target));
      }
    }

    // Render if forced or if the last render was over ~1ms ago.
    bool do_render = force_render || timer.LastFrameRenderedMicrosAgo() > 1200;
    if (!do_render) {
      continue;
    }

    timer.OnStartRender();
    auto end_render_guard = ScopeGuard::Create([&] { timer.OnEndRender(); });

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();
    DrawCrosshair(crosshair, screen, draw_list);

    float elapsed_seconds = timer.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    DrawFrame(app, &_target_manager, &sphere_renderer, &room, look_at.transform);
  }

  float hit_percent = stats.targets_hit / (float)stats.shots_taken;
  float score = stats.targets_hit * 10 * sqrt(hit_percent);

  std::string score_string = std::format(
      "{}/{} ({:.1f}%) = {:.2f}", stats.targets_hit, stats.shots_taken, hit_percent * 100, score);

  // Replay
  PlayReplay(replay, app, score_string);

  ReplayFileT replay_file;
  replay_file.replay.Set(replay);
  flatbuffers::FlatBufferBuilder fbb;
  fbb.Finish(ReplayFile::Pack(fbb, &replay_file));

  std::ofstream outfile("C:/Users/micha/replay1.bin", std::ios::binary);
  outfile.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
  outfile.close();
}

}  // namespace aim

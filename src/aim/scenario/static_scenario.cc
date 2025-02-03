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

bool PlayReplay(const StaticReplayT& replay, Application* app, const std::string& score_string) {
  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = GetPerspectiveTransformation(screen);

  auto stored_camera_position = replay.camera_position.get();
  glm::vec3 camera_position = ToVec3(*stored_camera_position);

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
        return true;
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_ESCAPE) {
          return false;
        }
      }
    }
    timer.OnStartFrame();

    uint64_t replay_frame_number = timer.GetReplayFrameNumber();
    if (replay_frame_number >= replay_frames.size()) {
      return false;
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
      app->GetSoundManager()->PlayKillSound().PlayShootSound();
    } else if (has_miss) {
      app->GetSoundManager()->PlayShootSound();
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
  return false;
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
glm::vec2 GetNewTargetPosition(const RoomParams& room_params,
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
          return GetRandomPositionInCircle(0.5 * room_params.wall_width * region.x_circle_percent,
                                           0.5 * room_params.wall_height * region.y_circle_percent,
                                           app);
        }
        float max_x = 0.5 * room_params.wall_width * region.x_percent;
        float max_y = 0.5 * room_params.wall_height * region.y_percent;
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

Target GetNewTarget(const RoomParams& room_params,
                    const StaticScenarioParams& params,
                    TargetManager* target_manager,
                    Application* app) {
  glm::vec2 pos = GetNewTargetPosition(room_params, params.target_placement, target_manager, app);
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
    : camera_(GetInitialCamera(params)), params_(params) {}

void StaticScenario::Run(Application* app) {
  while (true) {
    target_manager_.Clear();
    bool restart = this->RunInternal(app);
    if (!restart) {
      return;
    }
  }
}

bool StaticScenario::RunInternal(Application* app) {
  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = GetPerspectiveTransformation(screen);
  float radians_per_dot = CmPer360ToRadiansPerDot(params_.cm_per_360, app->GetMouseDpi());
  auto crosshair = GetDefaultCrosshair();

  RoomParams room_params;
  room_params.wall_height = params_.room_height;
  room_params.wall_width = params_.room_width;

  StaticReplayT replay;
  uint16_t replay_frames_per_second = 100;
  replay.frames_per_second = replay_frames_per_second;
  replay.camera_position = ToStoredVec3Ptr(camera_.GetPosition());

  for (int i = 0; i < params_.num_targets; ++i) {
    Target target =
        target_manager_.AddTarget(GetNewTarget(room_params, params_, &target_manager_, app));

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

  int targets_hit = 0;
  int shots_taken = 0;

  // Set up metronome
  float target_number_of_hits_per_60 = 131;
  float seconds_per_target = params_.duration_seconds /
                             (target_number_of_hits_per_60 * (params_.duration_seconds / 60.0f));
  TimedInvokerParams metronome_params;
  metronome_params.interval_micros = seconds_per_target * 1000000;
  TimedInvoker metronome(metronome_params, [&] { app->GetSoundManager()->PlayMetronomeSound(); });

  // Main loop
  ScenarioTimer timer(replay_frames_per_second);
  bool stop_scenario = false;
  while (!stop_scenario) {
    timer.OnStartFrame();

    if (timer.IsNewReplayFrame()) {
      // Store the look at vector before the mouse updates for the old frame.
      replay.pitch_yaw_pairs.push_back(PitchYaw(camera_.GetPitch(), camera_.GetYaw()));
    }

    if (timer.GetElapsedSeconds() >= params_.duration_seconds) {
      stop_scenario = true;
      continue;
    }

    SDL_Event event;
    bool has_click = false;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return false;
      }
      if (event.type == SDL_EVENT_MOUSE_MOTION) {
        camera_.Update(event.motion.xrel, event.motion.yrel, radians_per_dot);
      }
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        has_click = true;
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_S) {
          stop_scenario = true;
        }
        if (keycode == SDLK_R) {
          return true;
        }
      }
    }

    // Update state

    metronome.MaybeInvoke(timer.GetElapsedMicros());
    bool force_render = false;
    look_at = camera_.GetLookAt();

    if (has_click) {
      shots_taken++;
      auto maybe_hit_target_id = target_manager_.GetNearestHitTarget(camera_, look_at.front);
      app->GetSoundManager()->PlayShootSound();
      if (maybe_hit_target_id.has_value()) {
        targets_hit++;
        app->GetSoundManager()->PlayKillSound();
        force_render = true;

        auto hit_target_id = *maybe_hit_target_id;
        Target new_target = target_manager_.ReplaceTarget(
            hit_target_id, GetNewTarget(room_params, params_, &target_manager_, app));

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

    DrawFrame(app, &target_manager_, &sphere_renderer, &room, look_at.transform);
  }

  float hit_percent = targets_hit / (float)shots_taken;
  float duration_modifier = 60.0f / params_.duration_seconds;
  float score = targets_hit * 10 * sqrt(hit_percent) * duration_modifier;

  std::string score_string =
      std::format("{}/{} ({:.1f}%) = {:.2f}", targets_hit, shots_taken, hit_percent * 100, score);

  StatsRow stats_row;
  stats_row.cm_per_360 = params_.cm_per_360;
  stats_row.num_hits = targets_hit;
  stats_row.num_kills = targets_hit;
  stats_row.num_shots = shots_taken;
  stats_row.score = score;
  if (stats_row.score > 0) {
    app->GetStatsDb()->AddStats(params_.scenario_id, &stats_row);
  }

  auto all_stats = app->GetStatsDb()->GetStats(params_.scenario_id);
  auto high_score_stats =
      std::max_element(all_stats.begin(), all_stats.end(), [&](auto& lhs, auto& rhs) {
        return lhs.score < rhs.score;
      });
  double high_score = high_score_stats->score;

  // Show results page
  SDL_GL_SetSwapInterval(1);  // Enable vsync
  SDL_SetWindowRelativeMouseMode(app->GetSdlWindow(), false);
  bool view_replay = false;
  while (true) {
    if (view_replay) {
      SDL_GL_SetSwapInterval(0);
      bool need_quit = PlayReplay(replay, app, score_string);
      if (need_quit) {
        return false;
      }
      SDL_GL_SetSwapInterval(1);
      view_replay = false;
      continue;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        return false;
      }
      if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode keycode = event.key.key;
        if (keycode == SDLK_ESCAPE) {
          return false;
        }
        if (keycode == SDLK_R) {
          return true;
        }
      }
    }

    ImDrawList* draw_list = app->StartFullscreenImguiFrame();

    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::Text("high_score: %.2f", high_score);
    ImGui::Text("total_runs: %d", all_stats.size());
    ImGui::Text("score: %s", score_string.c_str());
    ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);
    if (ImGui::Button("View replay", sz)) {
      view_replay = true;
    }
    if (ImGui::Button("Save replay", sz)) {
      ReplayFileT replay_file;
      replay_file.replay.Set(replay);
      flatbuffers::FlatBufferBuilder fbb;
      fbb.Finish(ReplayFile::Pack(fbb, &replay_file));
      std::ofstream outfile(std::format("C:/Users/micha/AimTrainer/replay_{}_{}.bin",
                                        params_.scenario_id,
                                        stats_row.stats_id),
                            std::ios::binary);
      outfile.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
      outfile.close();
    }
    ImGui::End();

    ImVec4 clear_color = ImVec4(0.7f, 0.7f, 0.7f, 1.00f);
    if (app->StartRender(clear_color)) {
      app->FinishRender();
    }
  }
  return false;

  /*
   */
}

}  // namespace aim

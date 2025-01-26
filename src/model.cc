#include "model.h"

#include <fstream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>

#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/intersect.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <random>

#include "application.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "camera.h"
#include "flatbuffers/flatbuffers.h"
#include "imgui.h"
#include "replay_generated.h"
#include "sound.h"
#include "time_util.h"
#include "util.h"

namespace aim {
namespace {

struct ReplayFrame {
  std::vector<AddTargetEventT*> add_target_events;
  std::vector<HitTargetEventT*> hit_target_events;
  std::vector<MissTargetEventT*> miss_target_events;
  StoredVec3 look_at;
};

std::vector<ReplayFrame> GetReplayFrames(const StaticReplayT& replay) {
  std::vector<ReplayFrame> replay_frames;
  for (int frame_number = 0; frame_number < replay.look_at_vectors.size(); ++frame_number) {
    ReplayFrame frame;
    frame.look_at = replay.look_at_vectors[frame_number];
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

}  // namespace

ImVec2 GetScreenPosition(const glm::vec3& target,
                         const glm::mat4& transform,
                         const ScreenInfo& screen) {
  glm::vec4 position = {target.x, target.y, target.z, 1.0};
  glm::vec4 clip_space = transform * position;

  // Perform perspective division
  glm::vec3 ndc_space = glm::vec3(clip_space) / clip_space.w;

  // Convert NDC space (-1 to 1) to screen space (0 to
  // width/height)
  float screen_x = (ndc_space.x + 1.0f) * 0.5f * screen.width;
  float screen_y = (1.0f - ndc_space.y) * 0.5f * screen.height;

  return ImVec2(screen_x, screen_y);
}

void Room::Draw(ImDrawList* draw_list, const glm::mat4& transform, const ScreenInfo& screen) {
  float max_x = wall_width * 0.5;
  float min_x = -1 * max_x;

  float max_y = wall_height * camera_height_percent;
  float min_y = max_y - wall_height;

  float wall_z = wall_distance * -1.0f;

  auto top_left = GetScreenPosition({min_x, max_y, wall_z}, transform, screen);
  auto top_right = GetScreenPosition({max_x, max_y, wall_z}, transform, screen);
  auto bottom_left = GetScreenPosition({min_x, min_y, wall_z}, transform, screen);
  auto bottom_right = GetScreenPosition({max_x, min_y, wall_z}, transform, screen);

  ImVec2 points[] = {top_left, top_right, bottom_right, bottom_left};
  draw_list->AddPolyline(points, 4, IM_COL32(238, 232, 213, 255), true, 5.f);
}

Scenario::Scenario() : _camera(Camera(glm::vec3(0, -100.0f, 0))) {
  std::random_device rd;
  _random_generator = std::mt19937(rd());
}

void PlayReplay(const StaticReplayT& replay, Application* app) {
  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = glm::perspective(
      glm::radians(103.0f), (float)screen.width / (float)screen.height, 50.0f, 2000.0f);

  auto stored_camera_position = replay.camera_position.get();
  glm::vec3 camera_position = ToVec3(*stored_camera_position);

  auto hit_sound = Sound::Load("blop1.ogg");

  std::vector<ReplayFrame> replay_frames = GetReplayFrames(replay);
  uint32_t replay_frame_number = 1000;

  float replay_seconds_per_frame = 1 / (float)replay.frames_per_second;
  uint64_t replay_micros_per_frame = replay_seconds_per_frame * 1000000;

  Stopwatch stopwatch;
  stopwatch.Start();

  uint64_t last_frame_start_time_micros = stopwatch.GetElapsedMicros();
  uint64_t frame_start_time_micros = stopwatch.GetElapsedMicros();

  std::unordered_map<uint16_t, Target> target_map;

  while (true) {
    last_frame_start_time_micros = frame_start_time_micros;
    frame_start_time_micros = stopwatch.GetElapsedMicros();

    uint32_t previous_replay_frame_number = replay_frame_number;
    replay_frame_number = frame_start_time_micros / replay_micros_per_frame;
    if (replay_frame_number >= replay_frames.size()) {
      return;
    }

    bool has_new_frame_number = previous_replay_frame_number != replay_frame_number;
    if (!has_new_frame_number) {
      continue;
    }

    auto& replay_frame = replay_frames[replay_frame_number];
    LookAtInfo look_at = GetLookAt(camera_position, ToVec3(replay_frame.look_at));
    auto transform = projection * look_at.transform;

    if (replay_frame.hit_target_events.size() > 0) {
      if (hit_sound) {
        hit_sound->Play();
      }
    }
    // Play miss sound.
    for (HitTargetEventT* event : replay_frame.hit_target_events) {
      target_map.erase(event->target_id);
    }
    for (AddTargetEventT* event : replay_frame.add_target_events) {
      Target t;
      t.id = event->target_id;
      t.radius = event->radius;
      t.position = ToVec3(*event->position);
      target_map[t.id] = t;
    }

    app->MaybeRebuildSwapChain();

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Get the drawing list and calculate center position
    // Create fullscreen window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)screen.width, (float)screen.height));
    ImGui::Begin("Fullscreen",
                 nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;

    auto right = look_at.right;
    for (const auto& [key, target] : target_map) {
      ImVec2 screen_pos = GetScreenPosition(target.position, transform, screen);
      // Draw circle

      glm::vec3 out_target = target.position + (right * target.radius);
      ImVec2 radius_pos = GetScreenPosition(out_target, transform, screen);

      float screen_radius = glm::length(ToVec2(screen_pos) - ToVec2(radius_pos));

      ImU32 circle_color = IM_COL32(255, 255, 255, 255);  // White color
      draw_list->AddCircleFilled(screen_pos, screen_radius, circle_color, 0);
    }

    {
      // crosshair
      float radius = 3.0f;
      ImU32 circle_color = IM_COL32(0, 0, 0, 255);
      draw_list->AddCircleFilled(screen.center, radius, circle_color, 0);
    }

    float elapsed_seconds = stopwatch.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    // Rendering
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized =
        (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized) {
      ImVec4 clear_color = ImVec4(0.45f, 0.25f, 0.60f, 1.00f);
      app->FrameRender(clear_color, draw_data);
    }
  }
}

void Scenario::Run(Application* app) {
  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = glm::perspective(
      glm::radians(103.0f), (float)screen.width / (float)screen.height, 50.0f, 2000.0f);
  SDL_SetWindowRelativeMouseMode(app->GetSdlWindow(), true);

  std::uniform_real_distribution<float> distribution_x(-100.0f, 100.0f);
  std::uniform_real_distribution<float> distribution_z(-40.0f, 40.0f);
  auto get_new_position = [&]() {
    double rx = distribution_x(_random_generator);
    double rz = distribution_z(_random_generator);
    return glm::vec3(rx, 0.0f, rz);
  };

  uint32_t replay_frame_number = 0;

  StaticReplayT replay;
  uint16_t replay_frames_per_second = 120;
  replay.frames_per_second = replay_frames_per_second;
  replay.camera_position = ToStoredVec3Ptr(_camera.GetPosition());

  float replay_seconds_per_frame = 1 / (float)replay_frames_per_second;
  uint64_t replay_micros_per_frame = replay_seconds_per_frame * 1000000;

  uint16_t target_counter = 1;

  for (int i = 0; i < 4; ++i) {
    Target target(target_counter++, get_new_position(), 2);
    _targets.push_back(target);

    // Add replay event
    auto add_target = std::make_unique<AddTargetEventT>();
    add_target->target_id = target.id;
    add_target->frame_number = replay_frame_number;
    add_target->position = ToStoredVec3Ptr(target.position);
    add_target->radius = target.radius;
    replay.add_target_events.push_back(std::move(add_target));
  }

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // TODO: needs to be cleaned up and paths / where to store things resolved.
  auto hit_sound = Sound::Load("blop1.ogg");
  float radians_per_dot = CmPer360ToRadiansPerDot(45, 1600);

  Stopwatch stopwatch;
  stopwatch.Start();

  uint64_t last_frame_start_time_micros = stopwatch.GetElapsedMicros();
  uint64_t frame_start_time_micros = stopwatch.GetElapsedMicros();
  uint64_t last_render_time_micros = 0;

  uint64_t frame_count = 0;
  bool draw_reference_square = false;

  LookAtInfo look_at;

  bool stop_scenario = false;
  while (!stop_scenario) {
    frame_count++;

    last_frame_start_time_micros = frame_start_time_micros;
    frame_start_time_micros = stopwatch.GetElapsedMicros();

    uint32_t previous_replay_frame_number = replay_frame_number;
    replay_frame_number = frame_start_time_micros / replay_micros_per_frame;
    bool has_new_frame_number = previous_replay_frame_number != replay_frame_number;

    if (has_new_frame_number) {
      // Store the look at vector before the mouse updates for the old frame.
      replay.look_at_vectors.push_back(ToStoredVec3(look_at.front));
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
        if (keycode == SDLK_D) {
          draw_reference_square = !draw_reference_square;
        }
        if (keycode == SDLK_S) {
          stop_scenario = true;
        }
      }
    }
    if (SDL_GetWindowFlags(app->GetSdlWindow()) & SDL_WINDOW_MINIMIZED) {
      // TODO: Pause the run.
      SDL_Delay(100);
      continue;
    }

    // Update state
    bool force_render = frame_count == 1;

    look_at = _camera.GetLookAt();
    auto transform = projection * look_at.transform;

    for (Target& target : _targets) {
      if (target.hidden) {
        continue;
      }
      if (has_click) {
        glm::vec3 intersection_point;
        glm::vec3 intersection_normal;
        bool is_hit = glm::intersectRaySphere(_camera.GetPosition(),
                                              look_at.front,
                                              target.position,
                                              target.radius,
                                              intersection_point,
                                              intersection_normal);
        if (is_hit) {
          auto hit_target_id = target.id;
          target.position = get_new_position();
          target.id = target_counter++;
          force_render = true;
          if (hit_sound) {
            hit_sound->Play();
          }

          // Add replay events
          auto add_target = std::make_unique<AddTargetEventT>();
          add_target->target_id = target.id;
          add_target->frame_number = replay_frame_number;
          add_target->position = ToStoredVec3Ptr(target.position);
          add_target->radius = target.radius;
          replay.add_target_events.push_back(std::move(add_target));

          auto hit_target = std::make_unique<HitTargetEventT>();
          hit_target->target_id = hit_target_id;
          hit_target->frame_number = replay_frame_number;
          replay.hit_target_events.push_back(std::move(hit_target));
        } else {
          // miss_sound->Play();
          auto miss_target = std::make_unique<MissTargetEventT>();
          miss_target->frame_number = replay_frame_number;
          replay.miss_target_events.push_back(std::move(miss_target));
        }
      }
    }

    // Maybe Render
    bool do_render = force_render || frame_count % 3 == 0;
    if (!do_render) {
      continue;
    }

    app->MaybeRebuildSwapChain();

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Get the drawing list and calculate center position
    // Create fullscreen window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)screen.width, (float)screen.height));
    ImGui::Begin("Fullscreen",
                 nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;

    if (draw_reference_square) {
      {
        auto top_left = GetScreenPosition({-100, 0, 100}, transform, screen);
        auto top_right = GetScreenPosition({100, 0, 100}, transform, screen);
        auto bottom_right = GetScreenPosition({100, 0, -100}, transform, screen);
        auto bottom_left = GetScreenPosition({-100, 0, -100}, transform, screen);

        ImVec2 points[] = {top_left, top_right, bottom_right, bottom_left};
        draw_list->AddPolyline(points, 4, IM_COL32(238, 232, 213, 255), true, 5.f);
      }
      {
        auto top_left = GetScreenPosition({-150, 0, 150}, transform, screen);
        auto top_right = GetScreenPosition({150, 0, 150}, transform, screen);
        auto bottom_right = GetScreenPosition({150, 0, -150}, transform, screen);
        auto bottom_left = GetScreenPosition({-150, 0, -150}, transform, screen);

        ImVec2 points[] = {top_left, top_right, bottom_right, bottom_left};
        draw_list->AddLine(top_left, top_right, IM_COL32(238, 0, 0, 255), 4);
        draw_list->AddLine(top_right, bottom_right, IM_COL32(0, 238, 0, 255), 4);
      }
    }

    auto right = look_at.right;
    for (Target& target : _targets) {
      if (!target.hidden) {
        ImVec2 screen_pos = GetScreenPosition(target.position, transform, screen);
        // Draw circle

        glm::vec3 out_target = target.position + (right * target.radius);
        ImVec2 radius_pos = GetScreenPosition(out_target, transform, screen);

        float screen_radius = glm::length(ToVec2(screen_pos) - ToVec2(radius_pos));

        ImU32 circle_color = IM_COL32(255, 255, 255, 255);  // White color
        draw_list->AddCircleFilled(screen_pos, screen_radius, circle_color, 0);
      }
    }

    {
      // crosshair
      float radius = 3.0f;
      ImU32 circle_color = IM_COL32(0, 0, 0, 255);
      draw_list->AddCircleFilled(screen.center, radius, circle_color, 0);
    }

    float elapsed_seconds = stopwatch.GetElapsedSeconds();
    ImGui::Text("time: %.1f", elapsed_seconds);
    ImGui::Text("fps: %d", (int)ImGui::GetIO().Framerate);
    ImGui::End();

    // Rendering
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized =
        (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized) {
      app->FrameRender(clear_color, draw_data);
    }
  }

  PlayReplay(replay, app);

  ReplayFileT replay_file;
  replay_file.replay.Set(replay);
  flatbuffers::FlatBufferBuilder fbb;
  fbb.Finish(ReplayFile::Pack(fbb, &replay_file));

   std::ofstream outfile("C:/Users/micha/replay1.bin", std::ios::binary);
  outfile.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
  outfile.close();
}

// Poll and handle events (inputs, window resize, etc.)
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui
// wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main
// application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main
// application, or clear/overwrite your copy of the keyboard data. Generally you may always pass
// all inputs to dear imgui, and hide them from your application based on those two flags.

}  // namespace aim

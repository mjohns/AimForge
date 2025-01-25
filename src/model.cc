#include "model.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>

#include <glm/gtc/matrix_transform.hpp>
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
#include "imgui.h"
#include "model.h"
#include "sound.h"
#include "time_util.h"
#include "util.h"

namespace aim {

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

Scenario::Scenario() : _camera(Camera(glm::vec3(0, -500.0f, 0))) {
  std::random_device rd;
  _random_generator = std::mt19937(rd());
}

void Scenario::Run(Application* app) {
  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = glm::perspective(
      glm::radians(103.0f), (float)screen.width / (float)screen.height, 50.0f, 2000.0f);
  SDL_SetWindowRelativeMouseMode(app->GetSdlWindow(), true);

  std::uniform_real_distribution<float> distribution_x(-500.0f, 500.0f);
  std::uniform_real_distribution<float> distribution_z(-120.0f, 120.0f);
  auto get_new_position = [&]() {
    double rx = distribution_x(_random_generator);
    double rz = distribution_z(_random_generator);
    return glm::vec3(rx, 0.0f, rz);
  };
  for (int i = 0; i < 4; ++i) {
    _targets.push_back(Target(get_new_position(), 10));
  }

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // TODO: needs to be cleaned up and paths / where to store things resolved.
  auto hit_sound = Sound::Load("blop1.ogg");
  float radians_per_dot = CmPer360ToRadiansPerPixel(50, 1600);

  Stopwatch stopwatch;
  stopwatch.Start();

  uint64_t last_frame_start_time_micros = stopwatch.GetElapsedMicros();
  uint64_t frame_start_time_micros = stopwatch.GetElapsedMicros();
  uint64_t last_render_time_micros = 0;

  uint64_t frame_count = 0;
  bool draw_reference_square = false;

  while (true) {
    frame_count++;

    last_frame_start_time_micros = frame_start_time_micros;
    frame_start_time_micros = stopwatch.GetElapsedMicros();

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
    }
    if (SDL_GetWindowFlags(app->GetSdlWindow()) & SDL_WINDOW_MINIMIZED) {
      // TODO: Pause the run.
      SDL_Delay(100);
      continue;
    }

    // Update state
    bool force_render = frame_count == 1;

    LookAtInfo look_at = _camera.GetLookAt();
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
          target.position = get_new_position();
          force_render = true;
          if (hit_sound) {
            hit_sound->Play();
          }
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
      // Draw circle
      float radius = 3.0f;
      ImU32 circle_color = IM_COL32(0, 0, 0, 255);
      draw_list->AddCircleFilled(screen.center, radius, circle_color, 0);
    }

    float elapsed_seconds = stopwatch.GetElapsedSeconds();
    ImGui::Text("Time: %.1f", elapsed_seconds);
    // ImGui::Text("fps: %d", (int)fps);
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

#include <SDL.h>
#include <SDL_vulkan.h>
#include <stdio.h>
#include <stdlib.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/trigonometric.hpp>
#include <random>

#include "application.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include "camera.h"
#include "imgui.h"
#include "model.h"
#include "util.h"

glm::vec3 GetRandomTargetPosition(
    float min_x, float max_x, float min_y, float max_y, float z, std::random_device& gen) {
  std::uniform_real_distribution<> dis_x(min_x, max_x);
  double rx = dis_x(gen);

  std::uniform_real_distribution<> dis_y(min_y, max_y);
  double ry = dis_y(gen);

  return glm::vec3((float)rx, (float)ry, z);
}

// Main code
int main(int, char**) {
  using namespace aim;
  auto app = Application::Create();

  ScreenInfo screen = app->GetScreenInfo();
  glm::mat4 projection = glm::perspective(
      glm::radians(103.0f), (float)screen.width / (float)screen.height, 3.0f, 5000.0f);

  // Seed the random number generator with current time for better randomness
  std::random_device rd;
  std::mt19937 gen(rd());

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  SDL_SetRelativeMouseMode(SDL_TRUE);

  Camera camera(0, glm::radians(90.0f));
  float radians_per_pixel = CmPer360ToRadiansPerPixel(50, 1600);

  float wall_height = 100.0;
  float wall_width = 110.0;
  float z = -100.0;
  float min_x = -0.5f * wall_width;
  float max_x = 0.5f * wall_width;
  float min_y = -0.5f * wall_height;
  float max_y = 0.5f * wall_height;

  std::vector<Target> targets = {
      {GetRandomTargetPosition(min_x, max_x, min_y, max_y, z, rd)},
      {GetRandomTargetPosition(min_x, max_x, min_y, max_y, z, rd)},
      {GetRandomTargetPosition(min_x, max_x, min_y, max_y, z, rd)},
      {GetRandomTargetPosition(min_x, max_x, min_y, max_y, z, rd)},
  };

  // Main loop
  bool done = false;
  while (!done) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui
    // wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main
    // application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main
    // application, or clear/overwrite your copy of the keyboard data. Generally you may always pass
    // all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    bool has_click = false;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) done = true;
      if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(app->GetSdlWindow())) {
        done = true;
      }
      if (event.type == SDL_MOUSEMOTION) {
        camera.Update(event.motion.xrel, event.motion.yrel, radians_per_pixel);
      }
      if (event.type == SDL_MOUSEBUTTONDOWN) {
        has_click = true;
      }
    }
    if (SDL_GetWindowFlags(app->GetSdlWindow()) & SDL_WINDOW_MINIMIZED) {
      SDL_Delay(100);
      continue;
    }

    app->MaybeRebuildSwapChain();

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
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

    auto transform = projection * camera.GetLookAt();

    float padding = 5;
    auto top_left = GetScreenPosition({min_x - padding, max_y + padding, z}, transform, screen);
    auto top_right = GetScreenPosition({max_x + padding, max_y + padding, z}, transform, screen);
    auto bottom_left = GetScreenPosition({min_x - padding, min_y - padding, z}, transform, screen);
    auto bottom_right = GetScreenPosition({max_x + padding, min_y - padding, z}, transform, screen);

    ImVec2 points[] = {top_left, top_right, bottom_right, bottom_left};
    draw_list->AddPolyline(points, 4, IM_COL32(238, 232, 213, 255), true, 5.f);

    auto right = camera.GetRight();
    for (Target& target : targets) {
      if (!target.hidden) {
		  ImVec2 screen_pos = GetScreenPosition(target.position, transform, screen);
        // Draw circle
        float world_radius = 2.0f;  // Adjust radius as needed

        glm::vec3 out_target = target.position + (right * world_radius);
        ImVec2 radius_pos = GetScreenPosition(out_target, transform, screen);

        float screen_radius = glm::length(ToVec2(screen_pos) - ToVec2(radius_pos));

        ImU32 circle_color = IM_COL32(255, 255, 255, 255);  // White color
        draw_list->AddCircleFilled(screen_pos, screen_radius, circle_color, 0);

        if (has_click) {
          glm::vec2 circle_pos = ToVec2(screen_pos);
          glm::vec2 crosshair_pos = ToVec2(screen.center);
          if (glm::length(circle_pos - crosshair_pos) <= screen_radius) {
            target.position = GetRandomTargetPosition(min_x, max_x, min_y, max_y, z, rd);
          }
        }
      }
    }

    {
      // crosshair
      // Draw circle
      float radius = 3.0f;
      ImU32 circle_color = IM_COL32(0, 0, 0, 255);
      draw_list->AddCircleFilled(screen.center, radius, circle_color, 0);
    }

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

  return 0;
}

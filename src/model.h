#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "imgui.h"

namespace aim {

struct ScreenInfo {
  ScreenInfo(int screen_width, int screen_height)
      : width(screen_width),
        height(screen_height),
        center(ImVec2(screen_width * 0.5, screen_height * 0.5)) {
  }

  int width;
  int height;
  ImVec2 center;
};

struct Target {
  glm::vec3 position;
  bool hidden = false;
};

struct Room {
  float wall_height = 0.0;
  float wall_width = 0.0;
  float wall_distance = 100.0;
  // Percentage of the wall_height up from the floor to place the camera.
  float camera_height_percent = 0.5;
};

ImVec2 GetScreenPosition(const glm::vec3& target,
                         const glm::mat4& transform,
                         const ScreenInfo& screen);

}  // namespace aim
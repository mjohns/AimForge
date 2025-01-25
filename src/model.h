#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <random>

#include "imgui.h"
#include "application.h"
#include "camera.h"

namespace aim {

struct Target {
  glm::vec3 position;
  float radius = 1.0f;
  bool hidden = false;
};

struct Room {
  float wall_height = 0.0;
  float wall_width = 0.0;
  float wall_distance = 100.0;
  // Percentage of the wall_height up from the floor to place the camera.
  float camera_height_percent = 0.5;

  void Draw(ImDrawList* draw_list, const glm::mat4& transform, const ScreenInfo& screen);
};

class Scenario {
 public:
  Scenario();

  void Run(Application* app);
 private:
  Camera _camera;
  std::vector<Target> _targets;

  std::mt19937 _random_generator;
};


ImVec2 GetScreenPosition(const glm::vec3& target,
                         const glm::mat4& transform,
                         const ScreenInfo& screen);

}  // namespace aim
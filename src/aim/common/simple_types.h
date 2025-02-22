#pragma once

#include <imgui.h>

#include <glm/vec3.hpp>

#include <cstdint>

namespace aim {

using u32 = uint32_t;
using u64 = uint64_t;
using u16 = uint16_t;

using i32 = int32_t;
using i64 = int64_t;
using i16 = int16_t;

struct Rgb {
  int r = 0;
  int g = 0;
  int b = 0;
};

struct Wall {
  float width;
  float height;
};

struct Cylinder {
  glm::vec3 top_position{};
  glm::vec3 bottom_position{};
  float radius = 0;
};

struct Pill {
  glm::vec3 top_position{};
  glm::vec3 bottom_position{};
  float radius = 0;
};

struct ScreenInfo {
  ScreenInfo(int screen_width, int screen_height)
      : width(screen_width),
        height(screen_height),
        center(ImVec2(screen_width * 0.5, screen_height * 0.5)) {}

  int width;
  int height;
  ImVec2 center;
};

}  // namespace aim
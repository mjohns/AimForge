#pragma once

#include <imgui.h>

#include <cstdint>

namespace aim {

using u32 = uint32_t;
using u64 = uint64_t;
using u16 = uint16_t;

using i32 = int32_t;
using i64 = int64_t;
using i16 = int16_t;

struct Wall {
  float height;
  float width;
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
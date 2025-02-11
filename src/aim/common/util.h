#pragma once

#include <imgui.h>
#include <stdlib.h>

#include <format>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>

#include "aim/common/simple_types.h"
#include "aim/proto/common.pb.h"

namespace aim {

template <typename T>
static ImVec2 ToImVec2(const T& v) {
  return ImVec2(v.x, v.y);
}

template <typename T>
static glm::vec2 ToVec2(const T& v) {
  return glm::vec2(v.x, v.y);
}

static glm::vec3 ToVec3(const StoredVec3& v) {
  return glm::vec3(v.x(), v.y(), v.z());
}

template <typename T>
static StoredVec3 ToStoredVec3(const T& v) {
  StoredVec3 result;
  result.set_x(v.x);
  result.set_y(v.y);
  result.set_z(v.z);
  return result;
}

template <typename T>
static StoredRgb ToStoredRgb(const T& c) {
  return ToStoredRgb(c.r, c.g, c.b);
}

static StoredRgb ToStoredRgb(i32 r, i32 g, i32 b) {
  StoredRgb result;
  result.set_r(r);
  result.set_g(g);
  result.set_b(b);
  return result;
}

static ImU32 ToImCol32(const StoredRgb& c, uint8_t alpha = 255) {
  return IM_COL32(c.r(), c.g(), c.b(), alpha);
}

static bool IsInt(float value) {
  int int_value = value;
  return (value - int_value) == 0;
}

static std::string MaybeIntToString(float value, int decimal_places = 1) {
  bool is_int = IsInt(value);
  if (is_int) {
    return std::format("{}", static_cast<int>(value));
  }
  if (decimal_places == 4) {
    return std::format("{:.4f}", value);
  }
  if (decimal_places == 3) {
    return std::format("{:.3f}", value);
  }
  if (decimal_places == 2) {
    return std::format("{:.2f}", value);
  }
  return std::format("{:.1f}", value);
}

static float ParseFloat(const std::string& text) {
  return strtod(text.c_str(), nullptr);
}

}  // namespace aim
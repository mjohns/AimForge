#pragma once

#include <imgui.h>
#include <stdlib.h>

#include <format>
#include <absl/strings/ascii.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "aim/common/simple_types.h"
#include "aim/proto/common.pb.h"

namespace aim {

constexpr const int kMaxRgbValue = 255;

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

static StoredVec3 ToStoredVec3(float x, float y, float z) {
  StoredVec3 result;
  result.set_x(x);
  result.set_y(y);
  result.set_z(z);
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

static StoredColor ToStoredColor(const std::string& hex) {
  StoredColor c;
  c.set_hex(hex);
  return c;
}

static StoredColor ToStoredColor(i32 r, i32 g, i32 b) {
  StoredColor result;
  result.set_r(r);
  result.set_g(g);
  result.set_b(b);
  return result;
}

static StoredColor ToStoredColor(float gray_value) {
  StoredColor result;
  int val = kMaxRgbValue * gray_value;
  result.set_r(val);
  result.set_g(val);
  result.set_b(val);
  return result;
}

static ImU32 ToImCol32(const StoredRgb& c, uint8_t alpha = 255) {
  return IM_COL32(c.r(), c.g(), c.b(), alpha);
}

static unsigned int HexStrToInt(const std::string& val) {
  unsigned int x;
  std::stringstream ss;
  ss << std::hex << val;
  ss >> x;
  return x;
}

static Rgb HexToRgb(std::string hex) {
  if (hex[0] == '#') {
    hex = hex.substr(1);
  }
  absl::AsciiStrToUpper(&hex);

  if (hex.length() != 6) {
    return Rgb();
  }
  Rgb v;
  v.r = HexStrToInt(hex.substr(0, 2));
  v.g = HexStrToInt(hex.substr(2, 2));
  v.b = HexStrToInt(hex.substr(4, 2));
  return v;
}

static ImU32 ToImCol32(const StoredColor& c) {
  int a = c.has_alpha() ? kMaxRgbValue * c.alpha() : kMaxRgbValue;
  if (c.has_hex()) {
    auto v = HexToRgb(c.hex());
    return IM_COL32(v.r, v.g, v.b, a);
  }

  return IM_COL32(c.r(), c.g(), c.b(), a);
}

static glm::vec4 ToVec4(const StoredColor& c) {
  float a = c.has_alpha() ? c.alpha() : 1.0;

  int r = c.r();
  int g = c.g();
  int b = c.b();
  if (c.has_hex()) {
    auto v = HexToRgb(c.hex());
    r = v.r;
    g = v.g;
    b = v.b;
  }

  return glm::vec4((float)r / kMaxRgbValue, (float)g / kMaxRgbValue, (float)b / kMaxRgbValue, a);
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
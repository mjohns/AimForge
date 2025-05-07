#include "util.h"

#include <absl/strings/ascii.h>
#include <google/protobuf/message.h>
#include <imgui.h>
#include <stdlib.h>

#include <algorithm>
#include <format>
#include <functional>
#include <glm/ext/scalar_common.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/proto/common.pb.h"

namespace aim {
namespace {

constexpr const int kMaxRgbValue = 255;

unsigned int HexStrToInt(const std::string& val) {
  unsigned int x;
  std::stringstream ss;
  ss << std::hex << val;
  ss >> x;
  return x;
}

}  // namespace

glm::vec3 ToVec3(const StoredVec3& v) {
  return glm::vec3(v.x(), v.y(), v.z());
}

glm::vec2 ToVec2(const StoredVec2& v) {
  return glm::vec2(v.x(), v.y());
}

StoredVec3 ToStoredVec3(float x, float y, float z) {
  StoredVec3 result;
  result.set_x(x);
  result.set_y(y);
  result.set_z(z);
  return result;
}

Rgb HexToRgb(std::string hex) {
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

StoredRgb ToStoredRgb(const StoredColor& c) {
  float mult = c.has_multiplier() ? c.multiplier() : 1.0f;
  int a = kMaxRgbValue;

  int r = c.r();
  int g = c.g();
  int b = c.b();
  if (c.has_hex()) {
    auto v = HexToRgb(c.hex());
    r = v.r;
    g = v.g;
    b = v.b;
  }
  StoredRgb result;
  result.set_r(glm::clamp((int)(r * mult), 0, kMaxRgbValue));
  result.set_g(glm::clamp((int)(g * mult), 0, kMaxRgbValue));
  result.set_b(glm::clamp((int)(b * mult), 0, kMaxRgbValue));
  return result;
}

StoredRgb ToStoredRgb(i32 r, i32 g, i32 b) {
  StoredRgb result;
  result.set_r(r);
  result.set_g(g);
  result.set_b(b);
  return result;
}

StoredColor ToStoredColor(const std::string& hex) {
  StoredColor c;
  c.set_hex(hex);
  return c;
}

StoredColor ToStoredColor(i32 r, i32 g, i32 b) {
  StoredColor result;
  result.set_r(r);
  result.set_g(g);
  result.set_b(b);
  return result;
}

StoredColor ToStoredColor(float gray_value) {
  StoredColor result;
  int val = kMaxRgbValue * gray_value;
  result.set_r(val);
  result.set_g(val);
  result.set_b(val);
  return result;
}

ImU32 ToImCol32(const StoredRgb& c, uint8_t alpha) {
  return IM_COL32(c.r(), c.g(), c.b(), alpha);
}

ImU32 ToImCol32(const StoredColor& c) {
  int a = kMaxRgbValue;
  StoredRgb v = ToStoredRgb(c);
  return IM_COL32(v.r(), v.g(), v.b(), a);
}

glm::vec3 ToVec3(const StoredColor& c) {
  StoredRgb v = ToStoredRgb(c);
  return glm::vec3(
      (float)v.r() / kMaxRgbValue, (float)v.g() / kMaxRgbValue, (float)v.b() / kMaxRgbValue);
}

bool IsInt(float value) {
  int int_value = value;
  return (value - int_value) == 0;
}

std::string MaybeIntToString(float value, int decimal_places) {
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

float ParseFloat(const std::string& text) {
  return strtod(text.c_str(), nullptr);
}

float GetJitteredValue(float base_value, float jitter, std::mt19937* random_generator) {
  if (jitter > 0) {
    auto dist = std::uniform_real_distribution<float>(-1, 1);
    float mult = dist(*random_generator);
    return base_value + jitter * mult;
  }
  return base_value;
}

float FirstNonZero(float v1, float v2) {
  return v1 != 0 ? v1 : v2;
}

bool FlipCoin(std::mt19937* random_generator) {
  auto dist = std::uniform_real_distribution<float>(0, 1);
  float roll = dist(*random_generator);
  return roll > 0.5;
}

float GetRandInRange(float min, float max, std::mt19937* random_generator) {
  auto dist = std::uniform_real_distribution<float>(min, max);
  return dist(*random_generator);
}

}  // namespace aim

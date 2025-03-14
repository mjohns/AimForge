#pragma once

#include <absl/strings/ascii.h>
#include <google/protobuf/message.h>
#include <imgui.h>
#include <stdlib.h>

#include <algorithm>
#include <format>
#include <functional>
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

static glm::vec2 ToVec2(const StoredVec2& v) {
  return glm::vec2(v.x(), v.y());
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
  int a = kMaxRgbValue;
  if (c.has_hex()) {
    auto v = HexToRgb(c.hex());
    return IM_COL32(v.r, v.g, v.b, a);
  }

  return IM_COL32(c.r(), c.g(), c.b(), a);
}

static glm::vec3 ToVec3(const StoredColor& c) {
  int r = c.r();
  int g = c.g();
  int b = c.b();
  if (c.has_hex()) {
    auto v = HexToRgb(c.hex());
    r = v.r;
    g = v.g;
    b = v.b;
  }

  return glm::vec3((float)r / kMaxRgbValue, (float)g / kMaxRgbValue, (float)b / kMaxRgbValue);
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

static float GetJitteredValue(float base_value, float jitter, std::mt19937* random_generator) {
  if (jitter > 0) {
    auto dist = std::uniform_real_distribution<float>(-1, 1);
    float mult = dist(*random_generator);
    return base_value + jitter * mult;
  }
  return base_value;
}

template <typename T>
static bool IsValidIndex(const T& list, int i) {
  return i >= 0 && i < list.size();
}

template <typename T>
static int ClampIndex(const T& list, int i) {
  if (i < 0) return 0;
  if (i < list.size()) return i;
  return list.size() - 1;
}

template <typename T>
static std::optional<T> GetValueIfPresent(const std::vector<T>& list, int i) {
  return IsValidIndex(list, i) ? list[i] : std::optional<T>{};
}

template <typename T>
static std::optional<T> GetValueIfPresent(const google::protobuf::RepeatedPtrField<T>& list,
                                          int i) {
  return IsValidIndex(list, i) ? list[i] : std::optional<T>{};
}

template <typename T>
static std::optional<T> FindValue(const std::vector<T>& list,
                                  std::function<bool(const T&)> predicate) {
  for (const T& value : list) {
    if (predicate(value)) {
      return value;
    }
  }
  return {};
}

template <typename T>
static std::optional<T> FindValue(const google::protobuf::RepeatedPtrField<T>& list,
                                  std::function<bool(const T&)> predicate) {
  for (const T& value : list) {
    if (predicate(value)) {
      return value;
    }
  }
  return {};
}

template <typename T>
static void PushBackAll(std::vector<T>* v, const std::vector<T>& values) {
  if (values.size() == 0) {
    return;
  }

  v->reserve(v->size() + values.size());
  v->insert(v->end(), values.begin(), values.end());
}

template <typename T>
bool VectorContains(const std::vector<T>& values, const T& value) {
  auto it = std::find(values.begin(), values.end(), value);
  return it != values.end();
}

static float FirstNonZero(float v1, float v2) {
  return v1 != 0 ? v1 : v2;
}

}  // namespace aim
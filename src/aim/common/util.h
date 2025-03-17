#pragma once

#include <absl/strings/ascii.h>
#include <google/protobuf/message.h>
#include <imgui.h>
#include <stdlib.h>

#include <algorithm>
#include <functional>
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

glm::vec3 ToVec3(const StoredVec3& v);
glm::vec3 ToVec3(const StoredColor& c);
glm::vec2 ToVec2(const StoredVec2& v);
StoredVec3 ToStoredVec3(float x, float y, float z);

template <typename T>
static ImVec2 ToImVec2(const T& v) {
  return ImVec2(v.x, v.y);
}

template <typename T>
static glm::vec2 ToVec2(const T& v) {
  return glm::vec2(v.x, v.y);
}

template <typename T>
static StoredVec3 ToStoredVec3(const T& v) {
  StoredVec3 result;
  result.set_x(v.x);
  result.set_y(v.y);
  result.set_z(v.z);
  return result;
}

Rgb HexToRgb(std::string hex);
StoredRgb ToStoredRgb(const StoredColor& c);
StoredRgb ToStoredRgb(i32 r, i32 g, i32 b);
StoredColor ToStoredColor(const std::string& hex);
StoredColor ToStoredColor(i32 r, i32 g, i32 b);
StoredColor ToStoredColor(float gray_value);
ImU32 ToImCol32(const StoredRgb& c, uint8_t alpha = 255);
ImU32 ToImCol32(const StoredColor& c);

bool IsInt(float value);
std::string MaybeIntToString(float value, int decimal_places = 1);

float ParseFloat(const std::string& text);
float GetJitteredValue(float base_value, float jitter, std::mt19937* random_generator);

float FirstNonZero(float v1, float v2);

template <typename T>
bool IsValidIndex(const T& list, int i) {
  return i >= 0 && i < list.size();
}

template <typename T>
int ClampIndex(const T& list, int i) {
  if (i < 0) return 0;
  if (i < list.size()) return i;
  return list.size() - 1;
}

template <typename T>
std::optional<T> GetValueIfPresent(const std::vector<T>& list, int i) {
  return IsValidIndex(list, i) ? list[i] : std::optional<T>{};
}

template <typename T>
std::optional<T> GetValueIfPresent(const google::protobuf::RepeatedPtrField<T>& list, int i) {
  return IsValidIndex(list, i) ? list[i] : std::optional<T>{};
}

template <typename T>
std::optional<T> FindValue(const std::vector<T>& list, std::function<bool(const T&)> predicate) {
  for (const T& value : list) {
    if (predicate(value)) {
      return value;
    }
  }
  return {};
}

template <typename T>
std::optional<T> FindValue(const google::protobuf::RepeatedPtrField<T>& list,
                           std::function<bool(const T&)> predicate) {
  for (const T& value : list) {
    if (predicate(value)) {
      return value;
    }
  }
  return {};
}

template <typename T>
void PushBackAll(std::vector<T>* v, const std::vector<T>& values) {
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

}  // namespace aim
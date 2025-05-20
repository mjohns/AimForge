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

i32 FloatColorTo255(float value);
Rgb HexToRgb(std::string hex);
std::string ToHexString(const StoredColor& c);
std::string ToHexString(const StoredRgb& c);
StoredRgb ToStoredRgb(const StoredColor& c);
StoredRgb ToStoredRgb(i32 r, i32 g, i32 b);
StoredColor ToStoredColor(const std::string& hex);
StoredColor ToStoredColor(i32 r, i32 g, i32 b);
StoredColor FloatToStoredColor(float r, float g, float b);
StoredRgb FloatToStoredRgb(float r, float g, float b);
StoredColor ToStoredColor(float gray_value);
ImU32 ToImCol32(const StoredRgb& c, uint8_t alpha = 255);
ImU32 ToImCol32(const StoredColor& c);

bool IsInt(float value);
std::string MaybeIntToString(float value, int decimal_places = 1);

float ParseFloat(const std::string& text);
float GetJitteredValue(float base_value, float jitter, std::mt19937* random_generator);

// Even chance of true or false.
bool FlipCoin(std::mt19937* random_generator);
float GetRandInRange(float min, float max, std::mt19937* random_generator);

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

template <typename T>
std::vector<T> MoveVectorItem(const std::vector<T>& original_values, int src_i, int dest_before_i) {
  if (!IsValidIndex(original_values, src_i)) {
    return original_values;
  }
  bool is_valid_dest = dest_before_i >= 0 && dest_before_i <= original_values.size();
  if (!is_valid_dest) {
    return original_values;
  }

  std::vector<T> result;

  // Copy over all values before dest_before_i except the item being moved.
  for (int i = 0; i < original_values.size() && i < dest_before_i; ++i) {
    if (i != src_i) {
      result.push_back(original_values[i]);
    }
  }
  result.push_back(original_values[src_i]);
  for (int i = dest_before_i; i < original_values.size(); ++i) {
    if (i != src_i) {
      result.push_back(original_values[i]);
    }
  }
  return result;
}

template <typename T, typename R>
std::vector<R> MapVector(const std::vector<T>& values, std::function<R(const T&)> fn) {
  std::vector<R> result;
  result.reserve(values.size());
  for (const T& value : values) {
    result.push_back(fn(value));
  }
  return result;
}

std::string MakeUniqueName(const std::string& name, const std::vector<std::string>& used_names);

static bool IsZero(const StoredVec3& v) {
  return v.x() == 0 && v.y() == 0 && v.z() == 0;
}

}  // namespace aim
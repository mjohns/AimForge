#pragma once

#include <expected>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>

#include "flatbuffers/flatbuffers.h"
#include "imgui.h"
#include "replay_generated.h"

namespace aim {

// The value or an error.
template <typename T>
using Result = std::expected<T, std::string>;

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
  return StoredVec3(v.x, v.y, v.z);
}

template <typename T>
static std::unique_ptr<StoredVec3> ToStoredVec3Ptr(const T& v) {
  return std::make_unique<StoredVec3>(v.x, v.y, v.z);
}

}  // namespace aim
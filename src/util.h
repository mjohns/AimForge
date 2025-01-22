#pragma once

#include <expected>
#include <string>
#include <glm/vec2.hpp>

#include "imgui.h"

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

}  // namespace aim
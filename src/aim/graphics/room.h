#pragma once

#include <glm/mat4x4.hpp>

#include "aim/graphics/shader.h"

namespace aim {

struct RoomParams {
  float wall_height = 0.0;
  float wall_width = 0.0;
};

struct Room {
 public:
  explicit Room(RoomParams params);

  void SetProjection(const glm::mat4& projection);
  void Draw(const glm::mat4& view);

  float GetWidth() {
    return params_.wall_width;
  }

  float GetHeight() {
    return params_.wall_height;
  }

 private:
  RoomParams params_;
  unsigned int vbo_;
  unsigned int vao_;
  Shader shader_;
};

}  // namespace aim
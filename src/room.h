#pragma once

#include <glm/mat4x4.hpp>

#include "shader.h"

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

 private:
  RoomParams _params;
  unsigned int _vbo;
  unsigned int _vao;
  Shader _shader;
};

}  // namespace aim
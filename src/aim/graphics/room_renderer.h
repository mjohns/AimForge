#pragma once

#include <glm/mat4x4.hpp>

#include "aim/graphics/shader.h"

namespace aim {

struct RoomRenderer {
 public:
  RoomRenderer();

  void SetProjection(const glm::mat4& projection);
  void Draw(float height, float width, const glm::mat4& view);

 private:
  unsigned int vbo_;
  unsigned int vao_;
  Shader shader_;
};

}  // namespace aim
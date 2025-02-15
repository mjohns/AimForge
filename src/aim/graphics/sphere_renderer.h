#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vector>

#include "aim/graphics/shader.h"

namespace aim {

struct Sphere {
  glm::vec3 position;
  float radius;
};

class SphereRenderer {
 public:
  SphereRenderer();

  void SetProjection(const glm::mat4& projection);
  void Draw(const glm::mat4& view, const glm::vec3& color, const std::vector<Sphere>& spheres);

 private:
  unsigned int vbo_;
  unsigned int vao_;
  unsigned int num_vertices_;
  Shader shader_;
};

}  // namespace aim
#pragma once

#include <glm/mat4x4.hpp>
#include <vector>

#include "glm/vec3.hpp"
#include "shader.h";

namespace aim {

struct Sphere {
  glm::vec3 position;
  float radius;
};

class SphereRenderer {
 public:
  SphereRenderer();

  void SetProjection(const glm::mat4& projection);
  void Draw(const glm::mat4& view, const std::vector<Sphere>& spheres);

 private:
  unsigned int _vbo;
  unsigned int _vao;
  unsigned int _instance_vbo;
  unsigned int _instance_vao;
  unsigned int _num_vertices;
  Shader _shader;
};

}  // namespace aim
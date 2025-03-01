#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vector>

#include "aim/common/simple_types.h"
#include "aim/graphics/shader.h"

namespace aim {

struct RenderableSphere {
  Sphere sphere;
  glm::vec3 color;
};

class SphereRenderer {
 public:
  SphereRenderer();
  ~SphereRenderer();

  void SetProjection(const glm::mat4& projection);
  void Draw(const glm::mat4& view, const std::vector<RenderableSphere>& spheres);

 private:
  unsigned int vbo_;
  unsigned int vao_;
  unsigned int num_vertices_;

  unsigned int instance_vbo_;

  Shader shader_;
};

}  // namespace aim
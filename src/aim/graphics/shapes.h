#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <vector>

namespace aim {

std::vector<float> GenerateSphereVertices(int num_subdivisions);

struct VertexAndTexCoord {
  glm::vec3 vertex{};
  glm::vec2 tex_coord{};
};

std::vector<VertexAndTexCoord> GenerateCylinderWallVertices(int num_segments);

} // namespace aim

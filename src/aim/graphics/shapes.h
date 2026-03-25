#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>

namespace aim {

std::vector<float> GenerateSphereVertices(int num_subdivisions);
std::vector<glm::vec3> GenerateCircleVertices(int num_segments);

struct VertexAndTexCoord {
  glm::vec3 vertex{};
  glm::vec2 tex_coord{};
};

std::vector<VertexAndTexCoord> GenerateCylinderWallVertices(int num_segments);
std::vector<VertexAndTexCoord> GenerateQuadVertices();

std::vector<glm::vec3> GenerateCylinderVertices(int num_segments);

}  // namespace aim

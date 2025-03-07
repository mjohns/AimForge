#include "shapes.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace aim {
namespace {

void PushFloats(std::vector<float>* list,
                const glm::vec3& v1,
                const glm::vec3& v2,
                const glm::vec3& v3) {
  list->push_back(v1.x);
  list->push_back(v1.y);
  list->push_back(v1.z);

  list->push_back(v2.x);
  list->push_back(v2.y);
  list->push_back(v2.z);

  list->push_back(v3.x);
  list->push_back(v3.y);
  list->push_back(v3.z);
}

std::vector<float> GenerateSphereVertices(std::vector<float>&& vertices, int num_subdivisions) {
  if (num_subdivisions <= 0) {
    return std::move(vertices);
  }
  std::vector<float> new_vertices;
  new_vertices.reserve(vertices.size() * 4);
  for (int j = 0; j < vertices.size() / 9; ++j) {
    int i = j * 9;
    glm::vec3 v1(vertices[i], vertices[i + 1], vertices[i + 2]);
    glm::vec3 v2(vertices[i + 3], vertices[i + 4], vertices[i + 5]);
    glm::vec3 v3(vertices[i + 6], vertices[i + 7], vertices[i + 8]);

    glm::vec v1_2 = glm::normalize(v1 + ((v2 - v1) * 0.5f));
    glm::vec v2_3 = glm::normalize(v2 + ((v3 - v2) * 0.5f));
    glm::vec v3_1 = glm::normalize(v3 + ((v1 - v3) * 0.5f));

    PushFloats(&new_vertices, v1_2, v2, v2_3);
    PushFloats(&new_vertices, v1, v1_2, v3_1);
    PushFloats(&new_vertices, v3, v3_1, v2_3);
    PushFloats(&new_vertices, v2_3, v3_1, v1_2);
  }
  return GenerateSphereVertices(std::move(new_vertices), num_subdivisions - 1);
}

void PushFloats(std::vector<float>* list, const std::vector<glm::vec3>& p, int i1, int i2, int i3) {
  PushFloats(list, p[i1], p[i2], p[i3]);
}

}  // namespace

std::vector<float> GenerateSphereVertices(int num_subdivisions) {
  float t = (1.0 + sqrt(5.0)) / 2.0;

  // Define the 12 vertices of an icosahedron
  std::vector<glm::vec3> p = {
      glm::normalize(glm::vec3(-1, t, 0)),
      glm::normalize(glm::vec3(1, t, 0)),
      glm::normalize(glm::vec3(-1, -t, 0)),
      glm::normalize(glm::vec3(1, -t, 0)),
      glm::normalize(glm::vec3(0, -1, t)),
      glm::normalize(glm::vec3(0, 1, t)),
      glm::normalize(glm::vec3(0, -1, -t)),
      glm::normalize(glm::vec3(0, 1, -t)),
      glm::normalize(glm::vec3(t, 0, -1)),
      glm::normalize(glm::vec3(t, 0, 1)),
      glm::normalize(glm::vec3(-t, 0, -1)),
      glm::normalize(glm::vec3(-t, 0, 1)),
  };

  /*
  std::vector<float> vertices = {
      -100, 0, -100,
      100, 0, -100,
      0, 0, 100,
  };
  return vertices;
  */
  std::vector<float> vertices;
  PushFloats(&vertices, p, 0, 11, 5);
  PushFloats(&vertices, p, 0, 5, 1);
  PushFloats(&vertices, p, 0, 1, 7);
  PushFloats(&vertices, p, 0, 7, 10);
  PushFloats(&vertices, p, 0, 10, 11);
  PushFloats(&vertices, p, 1, 5, 9);
  PushFloats(&vertices, p, 5, 11, 4);
  PushFloats(&vertices, p, 11, 10, 2);
  PushFloats(&vertices, p, 10, 7, 6);
  PushFloats(&vertices, p, 7, 1, 8);
  PushFloats(&vertices, p, 3, 9, 4);
  PushFloats(&vertices, p, 3, 4, 2);
  PushFloats(&vertices, p, 3, 2, 6);
  PushFloats(&vertices, p, 3, 6, 8);
  PushFloats(&vertices, p, 3, 8, 9);
  PushFloats(&vertices, p, 4, 9, 5);
  PushFloats(&vertices, p, 2, 4, 11);
  PushFloats(&vertices, p, 6, 2, 10);
  PushFloats(&vertices, p, 8, 6, 7);
  PushFloats(&vertices, p, 9, 8, 1);

  return GenerateSphereVertices(std::move(vertices), num_subdivisions);
}

void PushFloats(std::vector<float>* list,
                const glm::vec3& v1,
                const glm::vec2& t1,
                const glm::vec3& v2,
                const glm::vec2& t2,
                const glm::vec3& v3,
                const glm::vec2& t3) {
  list->push_back(v1.x);
  list->push_back(v1.y);
  list->push_back(v1.z);
  list->push_back(t1.x);
  list->push_back(t1.y);

  list->push_back(v2.x);
  list->push_back(v2.y);
  list->push_back(v2.z);
  list->push_back(t2.x);
  list->push_back(t2.y);

  list->push_back(v3.x);
  list->push_back(v3.y);
  list->push_back(v3.z);
  list->push_back(t3.x);
  list->push_back(t3.y);
}

std::vector<VertexAndTexCoord> GenerateCylinderWallVertices(int num_segments) {
  const float radians_per_segment = glm::two_pi<float>() / (float)num_segments;
  const float cos_theta = cos(radians_per_segment);
  const float sin_theta = sin(radians_per_segment);

  std::vector<VertexAndTexCoord> vertices;
  vertices.reserve(num_segments * 6);

  glm::vec2 current_point(0, -1);
  float tx_step = 1.0f / num_segments;
  float ty_top = 0.0f;
  float ty_bottom = 1.0f;
  for (int i = 0; i < num_segments; ++i) {
    float next_x = current_point.x * cos_theta - current_point.y * sin_theta;
    float next_y = current_point.x * sin_theta + current_point.y * cos_theta;

    float tx_right = i * tx_step;
    float tx_left = tx_right + tx_step;

    glm::vec2 next_point(next_x, next_y);

    VertexAndTexCoord bottom_right;
    VertexAndTexCoord top_right;
    VertexAndTexCoord bottom_left;
    VertexAndTexCoord top_left;

    bottom_right.vertex = glm::vec3(current_point.x, current_point.y, -0.5);
    top_right.vertex = glm::vec3(current_point.x, current_point.y, 0.5);

    bottom_left.vertex = glm::vec3(next_point.x, next_point.y, -0.5);
    top_left.vertex = glm::vec3(next_point.x, next_point.y, 0.5);

    bottom_right.tex_coord = glm::vec2(tx_right, ty_bottom);
    top_right.tex_coord = glm::vec2(tx_right, ty_top);

    bottom_left.tex_coord = glm::vec2(tx_left, ty_bottom);
    top_left.tex_coord = glm::vec2(tx_left, ty_top);

    vertices.push_back(bottom_right);
    vertices.push_back(top_right);
    vertices.push_back(bottom_left);

    vertices.push_back(top_right);
    vertices.push_back(top_left);
    vertices.push_back(bottom_left);

    current_point = next_point;
  }

  return vertices;
}

std::vector<glm::vec3> GenerateCylinderVertices(int num_segments) {
  const float radians_per_segment = glm::two_pi<float>() / (float)num_segments;
  const float cos_theta = cos(radians_per_segment);
  const float sin_theta = sin(radians_per_segment);

  std::vector<glm::vec3> vertices;
  vertices.reserve(num_segments * 6);

  glm::vec2 current_point(0, -1);
  float tx_step = 1.0f / num_segments;
  float ty_top = 1.0f;
  float ty_bottom = 0.0f;
  for (int i = 0; i < num_segments; ++i) {
    float next_x = current_point.x * cos_theta - current_point.y * sin_theta;
    float next_y = current_point.x * sin_theta + current_point.y * cos_theta;

    glm::vec2 next_point(next_x, next_y);

    glm::vec3 bottom_right(current_point.x, current_point.y, -0.5);
    glm::vec3 top_right(current_point.x, current_point.y, 0.5);

    glm::vec3 bottom_left(next_point.x, next_point.y, -0.5);
    glm::vec3 top_left(next_point.x, next_point.y, 0.5);

    // Add two triangles.
    vertices.push_back(bottom_right);
    vertices.push_back(bottom_left);
    vertices.push_back(top_right);

    vertices.push_back(top_right);
    vertices.push_back(bottom_left);
    vertices.push_back(top_left);

    current_point = next_point;
  }

  return vertices;
}

}  // namespace aim

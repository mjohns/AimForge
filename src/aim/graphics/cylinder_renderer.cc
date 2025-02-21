#include "cylinder_renderer.h"

#include <glad/glad.h>

#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "aim/common/geometry.h"

namespace aim {
namespace {

const char* vertex_shader = R"AIMS(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;

void main() {
  gl_Position = projection * view * model * vec4(aPos, 1.0f);
}
)AIMS";

const char* fragment_shader = R"AIMS(
#version 330 core
out vec4 FragColor;

uniform vec3 quad_color;

void main() {
  FragColor = vec4(quad_color, 1.0f);
}
)AIMS";

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

std::vector<float> GenerateVertices(int num_segments) {
  const float radians_per_segment = glm::two_pi<float>() / (float)num_segments;
  const float cos_theta = cos(radians_per_segment);
  const float sin_theta = sin(radians_per_segment);

  std::vector<float> vertices;
  vertices.reserve(num_segments * 6 * 3);

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
    PushFloats(&vertices, bottom_right, bottom_left, top_right);
    PushFloats(&vertices, top_right, bottom_left, top_left);

    current_point = next_point;
  }

  return vertices;
}

}  // namespace

CylinderRenderer::CylinderRenderer() : shader_(Shader(vertex_shader, fragment_shader)) {
  std::vector<float> vertices = GenerateVertices(100);
  num_vertices_ = vertices.size() / 3;

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);

  glBindVertexArray(vao_);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
}

void CylinderRenderer::SetProjection(const glm::mat4& projection) {
  shader_.Use();
  shader_.SetMat4("projection", projection);
}

void CylinderRenderer::Draw(const glm::vec3& camera_position,
                            const glm::mat4& view,
                            const glm::vec3& color,
                            const std::vector<Cylinder>& shapes) {
  shader_.Use();
  shader_.SetMat4("view", view);

  shader_.SetVec3("quad_color", color);

  for (const auto& c : shapes) {
    glm::mat4 model(1.f);
    model = glm::translate(model, c.position);
    if (c.up != glm::vec3(0, 0, 1)) {
      glm::vec3 z_axis = c.up;
      glm::vec3 y_axis = glm::normalize(glm::cross(z_axis, glm::vec3(0, 0, 1)));
      glm::vec3 x_axis = glm::normalize(glm::cross(z_axis, y_axis));
      model = model * MakeCoordinateSystemTransform(x_axis, y_axis, z_axis);
    }
    model = glm::scale(model, glm::vec3(c.radius, c.radius, c.height));
    shader_.SetMat4("model", model);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, num_vertices_);
  }
}

}  // namespace aim

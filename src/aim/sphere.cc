#include "sphere.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "glad/glad.h"

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

}  // namespace

SphereRenderer::SphereRenderer() : _shader(Shader(vertex_shader, fragment_shader)) {
  std::vector<float> vertices = GenerateSphereVertices(3);
  _num_vertices = vertices.size() / 3;

  glGenVertexArrays(1, &_vao);
  glGenBuffers(1, &_vbo);

  glBindVertexArray(_vao);

  glBindBuffer(GL_ARRAY_BUFFER, _vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
}

void SphereRenderer::SetProjection(const glm::mat4& projection) {
  _shader.Use();
  _shader.SetMat4("projection", projection);
}
void SphereRenderer::Draw(const glm::mat4& view, const std::vector<Sphere>& spheres) {
  _shader.Use();
  _shader.SetMat4("view", view);

  glm::vec3 color(0.9, 0.2, 0.9);
  _shader.SetVec3("quad_color", color);

  for (const auto& sphere : spheres) {
    glm::mat4 model(1.f);
    model = glm::translate(model, sphere.position);
    model = glm::scale(model, glm::vec3(sphere.radius));
    _shader.SetMat4("model", model);

    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, _num_vertices);
  }
}

}  // namespace aim
#include "sphere_renderer.h"

#include <glad/glad.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

namespace aim {
namespace {

const char* vertex_shader = R"AIMS(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in mat4 model;

uniform mat4 view;
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
}j
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

SphereRenderer::SphereRenderer() : shader_(Shader(vertex_shader, fragment_shader)) {
  std::vector<float> vertices = GenerateSphereVertices(3);
  num_vertices_ = vertices.size() / 3;

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glGenBuffers(1, &instance_vbo_);

  glBindVertexArray(vao_);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // instanced mat4 model transform
  glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
  glVertexAttribDivisor(1, 1);

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
  glVertexAttribDivisor(2, 1);

  glEnableVertexAttribArray(3);
  glVertexAttribPointer(
      3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
  glVertexAttribDivisor(3, 1);

  glEnableVertexAttribArray(4);
  glVertexAttribPointer(
      4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));
  glVertexAttribDivisor(4, 1);
}

SphereRenderer::~SphereRenderer() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteBuffers(1, &vbo_);
  glDeleteBuffers(1, &instance_vbo_);
}

void SphereRenderer::SetProjection(const glm::mat4& projection) {
  shader_.Use();
  shader_.SetMat4("projection", projection);
}

void SphereRenderer::Draw(const glm::mat4& view,
                          const glm::vec3& color,
                          const std::vector<Sphere>& spheres) {
  if (spheres.size() == 0) {
    return;
  }
  shader_.Use();
  shader_.SetMat4("view", view);
  shader_.SetVec3("quad_color", color);

  std::vector<glm::mat4> model_transforms;
  for (const Sphere& sphere : spheres) {
    glm::mat4 model(1.f);
    model = glm::translate(model, sphere.position);
    model = glm::scale(model, glm::vec3(sphere.radius));
    model_transforms.push_back(model);
  }

  glBindVertexArray(vao_);

  // instanced model transformation attribute
  glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
  glBufferData(GL_ARRAY_BUFFER,
               model_transforms.size() * sizeof(glm::mat4),
               model_transforms.data(),
               GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glDrawArraysInstanced(GL_TRIANGLES, 0, num_vertices_, model_transforms.size());
}

}  // namespace aim
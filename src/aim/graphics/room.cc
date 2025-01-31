#include "room.h"

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

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

constexpr float kMaxDistance = 2000.0f;
constexpr float kHalfMaxDistance = 1000.0f;

}  // namespace

Room::Room(RoomParams params) : _params(params), _shader(Shader(vertex_shader, fragment_shader)) {
  // clang-format off
  float wall_vertices[] = {
      0.5f, 0.0f, -0.5f,
      0.5f, 0.0f, 0.5f,
      -0.5f, 0.0f, 0.5f,

      -0.5f, 0.0f, 0.5f,
      -0.5f, 0.0f, -0.5f,
      0.5f, 0.0f, -0.5f,
    };
  // clang-format on

  glGenVertexArrays(1, &_vao);
  glGenBuffers(1, &_vbo);

  glBindVertexArray(_vao);

  glBindBuffer(GL_ARRAY_BUFFER, _vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(wall_vertices), wall_vertices, GL_STATIC_DRAW);

  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
}

void Room::SetProjection(const glm::mat4& projection) {
  _shader.Use();
  _shader.SetMat4("projection", projection);
}

void Room::Draw(const glm::mat4& view) {
  _shader.Use();
  _shader.SetMat4("view", view);

  glm::vec3 wall_color(0.7);
  glm::vec3 floor_color(0.6);
  glm::vec3 top_color(0.6);
  glm::vec3 side_color(0.65);

  {
    // Back wall 
    _shader.SetVec3("quad_color", wall_color);

    glm::mat4 model(1.f);
    model = glm::scale(model, glm::vec3(_params.wall_width, 1.0f, _params.wall_height));
    _shader.SetMat4("model", model);
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  {
    // Floor wall 
    _shader.SetVec3("quad_color", floor_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, -0.5 * kMaxDistance, -0.5 * _params.wall_height));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(_params.wall_width, 1.0f, kMaxDistance));
    _shader.SetMat4("model", model);

    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  {
    // Left wall 
    _shader.SetVec3("quad_color", side_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(-0.5 * _params.wall_width, -0.5 * kMaxDistance, 0));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, _params.wall_height));
    _shader.SetMat4("model", model);

    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  {
    // Right wall 
    _shader.SetVec3("quad_color", side_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0.5 * _params.wall_width, -0.5 * kMaxDistance, 0));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, _params.wall_height));
    _shader.SetMat4("model", model);

    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  {
    // Top wall 
    _shader.SetVec3("quad_color", top_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, -0.5 * kMaxDistance, 0.5 * _params.wall_height));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(_params.wall_width, 1.0f, kMaxDistance));
    _shader.SetMat4("model", model);

    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
}

}  // namespace aim

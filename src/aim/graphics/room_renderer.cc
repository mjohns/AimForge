#include "room_renderer.h"

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/proto/theme.pb.h"

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

constexpr const float kMaxDistance = 2000.0f;
constexpr const float kHalfMaxDistance = 1000.0f;

constexpr const int kQuadNumVertices = 6;

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

std::vector<float> GenerateCircularWallVertices(int num_segments) {
  const float radians_per_segment = glm::two_pi<float>() / (float)num_segments;
  const float cos_theta = cos(radians_per_segment);
  const float sin_theta = sin(radians_per_segment);

  std::vector<float> vertices;
  vertices.reserve(num_segments * 6 * 3);

  glm::vec2 current_point(0, 1);
  for (int i = 0; i < num_segments; ++i) {
    float next_x = current_point.x * cos_theta - current_point.y * sin_theta;
    float next_y = current_point.x * sin_theta + current_point.y * cos_theta;

    glm::vec2 next_point(next_x, next_y);

    glm::vec3 bottom_right(current_point.x, current_point.y, -0.5);
    glm::vec3 top_right(current_point.x, current_point.y, 0.5);

    glm::vec3 bottom_left(next_point.x, next_point.y, -0.5);
    glm::vec3 top_left(next_point.x, next_point.y, 0.5);

    // Add two triangles.
    PushFloats(&vertices, bottom_right, top_right, bottom_left);
    PushFloats(&vertices, top_right, top_left, bottom_left);

    current_point = next_point;
  }

  return vertices;
}

}  // namespace

RoomRenderer::RoomRenderer() : shader_(Shader(vertex_shader, fragment_shader)) {
  {
    // clang-format off
    float quad_vertices[] = {
          0.5f, 0.0f, -0.5f,
          0.5f, 0.0f, 0.5f,
          -0.5f, 0.0f, 0.5f,

          -0.5f, 0.0f, 0.5f,
          -0.5f, 0.0f, -0.5f,
          0.5f, 0.0f, -0.5f,
    };
    // clang-format on

    glGenVertexArrays(1, &quad_vao_);
    glGenBuffers(1, &quad_vbo_);

    glBindVertexArray(quad_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
  }
  {
    std::vector<float> vertices = GenerateCircularWallVertices(1000);
    circular_wall_num_vertices_ = vertices.size() / 3;

    glGenVertexArrays(1, &circular_wall_vao_);
    glGenBuffers(1, &circular_wall_vbo_);

    glBindVertexArray(circular_wall_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, circular_wall_vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
  }
}

void RoomRenderer::SetProjection(const glm::mat4& projection) {
  shader_.Use();
  shader_.SetMat4("projection", projection);
}

void RoomRenderer::Draw(const Room& room, const Theme& theme, const glm::mat4& view) {
  if (room.has_simple_room()) {
    DrawSimpleRoom(room.simple_room(), theme, view);
  }
  if (room.has_circular_room()) {
    DrawCircularRoom(room.circular_room(), theme, view);
  }
}

void RoomRenderer::DrawCircularRoom(const CircularRoom& room,
                                    const Theme& theme,
                                    const glm::mat4& view) {
  shader_.Use();
  shader_.SetMat4("view", view);

  glm::vec3 wall_color(ToVec4(theme.front_appearance().color()));
  glm::vec3 floor_color(ToVec4(theme.floor_appearance().color()));
  glm::vec3 top_color(ToVec4(theme.roof_appearance().color()));

  float quad_scale = room.radius() * 2.5;
  float height = room.height();

  {
    // Floor wall
    shader_.SetVec3("quad_color", floor_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, 0, -0.5 * height));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
    shader_.SetMat4("model", model);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, kQuadNumVertices);
  }

  {
    // Top wall
    shader_.SetVec3("quad_color", top_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, 0, 0.5 * height));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
    shader_.SetMat4("model", model);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, kQuadNumVertices);
  }

  {
    shader_.SetVec3("quad_color", wall_color);
    glm::mat4 model(1.f);
    model = glm::scale(model, glm::vec3(room.radius(), room.radius(), height));
    shader_.SetMat4("model", model);
    glBindVertexArray(circular_wall_vao_);
    glDrawArrays(GL_TRIANGLES, 0, circular_wall_num_vertices_);
  }
}

void RoomRenderer::DrawSimpleRoom(const SimpleRoom& room,
                                  const Theme& theme,
                                  const glm::mat4& view) {
  shader_.Use();
  shader_.SetMat4("view", view);

  i32 height = room.height();
  i32 width = room.width();

  glm::vec3 wall_color(ToVec4(theme.front_appearance().color()));
  glm::vec3 floor_color(ToVec4(theme.floor_appearance().color()));
  glm::vec3 top_color(ToVec4(theme.roof_appearance().color()));
  glm::vec3 side_color(ToVec4(theme.side_appearance().color()));

  {
    // Back wall
    shader_.SetVec3("quad_color", wall_color);

    glm::mat4 model(1.f);
    model = glm::scale(model, glm::vec3(width, 1.0f, height));
    shader_.SetMat4("model", model);
    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, kQuadNumVertices);
  }

  {
    // Floor wall
    shader_.SetVec3("quad_color", floor_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, -0.5 * kMaxDistance, -0.5 * height));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(width, 1.0f, kMaxDistance));
    shader_.SetMat4("model", model);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, kQuadNumVertices);
  }

  {
    // Left wall
    shader_.SetVec3("quad_color", side_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(-0.5 * width, -0.5 * kMaxDistance, 0));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
    shader_.SetMat4("model", model);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, kQuadNumVertices);
  }

  {
    // Right wall
    shader_.SetVec3("quad_color", side_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0.5 * width, -0.5 * kMaxDistance, 0));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
    shader_.SetMat4("model", model);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, kQuadNumVertices);
  }

  {
    // Top wall
    shader_.SetVec3("quad_color", top_color);

    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, -0.5 * kMaxDistance, 0.5 * height));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(width, 1.0f, kMaxDistance));
    shader_.SetMat4("model", model);

    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, kQuadNumVertices);
  }
}

}  // namespace aim

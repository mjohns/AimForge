#include "room_renderer.h"

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "aim/common/geometry.h"
#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/graphics/image.h"
#include "aim/proto/theme.pb.h"

namespace aim {
namespace {

const char* simple_vertex_shader = R"AIMS(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;

void main() {
  gl_Position = projection * view * model * vec4(aPos, 1.0f);
}
)AIMS";

const char* simple_fragment_shader = R"AIMS(
#version 330 core
out vec4 FragColor;

uniform vec3 quad_color;

void main() {
  FragColor = vec4(quad_color, 1.0f);
}
)AIMS";

const char* texture_vertex_shader = R"AIMS(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;

uniform vec2 tex_scale;

out vec2 TexCoord;

void main() {
  gl_Position = projection * view * model * vec4(aPos, 1.0f);
  TexCoord = vec2(aTexCoord.x * tex_scale.x, aTexCoord.y * tex_scale.y);
}
)AIMS";

const char* texture_fragment_shader = R"AIMS(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;
uniform vec3 mix_color;
uniform float mix_percent;

void main() {
  vec4 texColor = texture(texture1, TexCoord);
  vec3 mixedColor = mix(texColor.xyz, mix_color, mix_percent);
  FragColor = vec4(mixedColor.xyz, 1.0f);
}
)AIMS";

constexpr const float kMaxDistance = 2000.0f;
constexpr const float kHalfMaxDistance = 100.0f;

constexpr const int kQuadNumVertices = 6;

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

std::vector<float> GenerateCircularWallVertices(int num_segments) {
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

    float tx_right = i * tx_step;
    float tx_left = tx_right + tx_step;

    glm::vec2 next_point(next_x, next_y);

    glm::vec3 bottom_right(current_point.x, current_point.y, -0.5);
    glm::vec3 top_right(current_point.x, current_point.y, 0.5);

    glm::vec3 bottom_left(next_point.x, next_point.y, -0.5);
    glm::vec3 top_left(next_point.x, next_point.y, 0.5);

    glm::vec2 bottom_right_t(tx_right, ty_bottom);
    glm::vec2 top_right_t(tx_right, ty_top);

    glm::vec2 bottom_left_t(tx_left, ty_bottom);
    glm::vec2 top_left_t(tx_left, ty_top);

    // Add two triangles.
    PushFloats(&vertices,
               bottom_right,
               bottom_right_t,
               top_right,
               top_right_t,
               bottom_left,
               bottom_left_t);
    PushFloats(&vertices, top_right, top_right_t, top_left, top_left_t, bottom_left, bottom_left_t);

    current_point = next_point;
  }

  return vertices;
}

}  // namespace

RoomRenderer::~RoomRenderer() {
  glDeleteVertexArrays(1, &quad_vao_);
  glDeleteBuffers(1, &quad_vbo_);

  glDeleteVertexArrays(1, &circular_wall_vao_);
  glDeleteBuffers(1, &circular_wall_vbo_);
}

RoomRenderer::RoomRenderer(TextureManager* texture_manager)
    : simple_shader_(Shader(simple_vertex_shader, simple_fragment_shader)),
      texture_shader_(Shader(texture_vertex_shader, texture_fragment_shader)),
      texture_manager_(texture_manager) {
  {
    // clang-format off
    float quad_vertices[] = {
          // bottom right
          0.5f, 0.0f, -0.5f,  1.0f, 0.0f,
          // top right
          0.5f, 0.0f, 0.5f,   1.0f, 1.0f,
          //  top left
          -0.5f, 0.0f, 0.5f,  0.0f, 1.0f,

          //  top left
          -0.5f, 0.0f, 0.5f,  0.0f, 1.0f,
          // bottom left
          -0.5f, 0.0f, -0.5f,  0.0f, 0.0f,
          // bottom right
          0.5f, 0.0f, -0.5f,   1.0f, 0.0f,
    };
    // clang-format on

    glGenVertexArrays(1, &quad_vao_);
    glGenBuffers(1, &quad_vbo_);

    glBindVertexArray(quad_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texture attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
  }

  {
    std::vector<float> vertices = GenerateCircularWallVertices(1000);
    // 3 vertices plus 2 texture coords.
    circular_wall_num_vertices_ = vertices.size() / 5;

    glGenVertexArrays(1, &circular_wall_vao_);
    glGenBuffers(1, &circular_wall_vbo_);

    glBindVertexArray(circular_wall_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, circular_wall_vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texture attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
  }
}

void RoomRenderer::SetProjection(const glm::mat4& projection) {
  simple_shader_.Use();
  simple_shader_.SetMat4("projection", projection);

  texture_shader_.Use();
  texture_shader_.SetMat4("projection", projection);
}

void RoomRenderer::Draw(const Room& room, const Theme& theme, const glm::mat4& view) {
  if (room.has_simple_room()) {
    DrawSimpleRoom(room.simple_room(), theme, view);
  }
  if (room.has_circular_room()) {
    DrawCircularRoom(room.circular_room(), theme, view);
  }
  if (room.has_barrel_room()) {
    DrawBarrelRoom(room.barrel_room(), theme, view);
  }
}

void RoomRenderer::DrawWall(const glm::mat4& model,
                            const glm::mat4& view,
                            const Wall& wall,
                            const WallAppearance& appearance,
                            bool is_circular_wall) {
  if (appearance.has_texture()) {
    Texture* texture = texture_manager_->GetTexture(appearance.texture().texture_name());
    if (texture == nullptr) {
      // Too spammy to log this error?
      DrawWallSolidColor(model, view, glm::vec3(0.7), is_circular_wall);
      return;
    }

    glm::vec2 tex_scale;

    float tex_scale_height = 100;
    float tex_scale_width = texture->width() * (tex_scale_height / (float)texture->height());

    tex_scale.x = wall.width / tex_scale_width;
    tex_scale.y = wall.height / tex_scale_height;

    if (appearance.texture().has_scale()) {
      tex_scale *= appearance.texture().scale();
    }

    texture_shader_.Use();
    texture_shader_.SetMat4("view", view);

    texture_shader_.SetInt("texture1", 0);
    texture_shader_.SetMat4("model", model);
    texture_shader_.SetVec2("tex_scale", tex_scale);

    glm::vec3 mix_color = ToVec3(appearance.texture().mix_color());
    texture_shader_.SetVec2("mix_color", mix_color);
    texture_shader_.SetFloat("mix_percent", appearance.texture().mix_percent());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->id());
    if (is_circular_wall) {
      glBindVertexArray(circular_wall_vao_);
      glDrawArrays(GL_TRIANGLES, 0, circular_wall_num_vertices_);
    } else {
      glBindVertexArray(quad_vao_);
      glDrawArrays(GL_TRIANGLES, 0, kQuadNumVertices);
    }
    return;
  }

  DrawWallSolidColor(model, view, ToVec3(appearance.color()), is_circular_wall);
}

void RoomRenderer::DrawWallSolidColor(const glm::mat4& model,
                                      const glm::mat4& view,
                                      const glm::vec3& color,
                                      bool is_circular_wall) {
  simple_shader_.Use();
  simple_shader_.SetMat4("view", view);
  simple_shader_.SetVec3("quad_color", color);
  simple_shader_.SetMat4("model", model);
  if (is_circular_wall) {
    glBindVertexArray(circular_wall_vao_);
    glDrawArrays(GL_TRIANGLES, 0, circular_wall_num_vertices_);
  } else {
    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLES, 0, kQuadNumVertices);
  }
}

void RoomRenderer::DrawCircularRoom(const CircularRoom& room,
                                    const Theme& theme,
                                    const glm::mat4& view) {
  float quad_scale = room.radius() * 2.5;
  float height = room.height();

  {
    // Floor wall
    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, 0, -0.5 * height));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
    DrawWall(model, view, {quad_scale, quad_scale}, theme.floor_appearance());
  }

  {
    // Top wall
    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, 0, 0.5 * height));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
    DrawWall(model, view, {quad_scale, quad_scale}, theme.roof_appearance());
  }

  if (!room.hide_sides()) {
    float width = room.width();
    float perimeter = room.radius() * glm::two_pi<float>();

    float radians = (width / perimeter) * glm::pi<float>();
    glm::vec2 to_rotate(0, room.radius());
    float side_angle_degrees = room.has_side_angle_degrees() ? room.side_angle_degrees() : 20.0f;

    {
      // Left
      glm::vec2 left = RotateRadians(to_rotate, radians);
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(left.x, left.y, 0));
      model = glm::rotate(model, glm::radians(90.0f + side_angle_degrees), glm::vec3(0, 0, 1));
      model = glm::translate(model, glm::vec3(-1 * kHalfMaxDistance, 0, 0));
      model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
      DrawWall(model, view, {kMaxDistance, height}, theme.side_appearance());
    }

    {
      // Right
      glm::vec2 right = RotateRadians(to_rotate, -1 * radians);
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(right.x, right.y, 0));
      model = glm::rotate(model, glm::radians(-90.0f - side_angle_degrees), glm::vec3(0, 0, 1));
      model = glm::translate(model, glm::vec3(kHalfMaxDistance, 0, 0));
      model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
      DrawWall(model, view, {kMaxDistance, height}, theme.side_appearance());
    }
  }

  {
    glm::mat4 model(1.f);
    model = glm::scale(model, glm::vec3(room.radius(), room.radius(), height));
    DrawWall(model,
             view,
             {glm::two_pi<float>() * room.radius(), height},
             theme.front_appearance(),
             /* is_circular_wall= */ true);
  }
}

void RoomRenderer::DrawBarrelRoom(const BarrelRoom& room,
                                  const Theme& theme,
                                  const glm::mat4& view) {
  float quad_scale = room.radius() * 2.1;

  {
    // Front wall
    glm::mat4 model(1.f);
    model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
    DrawWall(model, view, {quad_scale, quad_scale}, theme.front_appearance());
  }

  {
    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, -0.5 * kMaxDistance, 0));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(room.radius(), room.radius(), kMaxDistance));
    DrawWall(model,
             view,
             {glm::two_pi<float>() * room.radius(), kMaxDistance},
             theme.side_appearance(),
             /* is_circular_wall= */ true);
  }
}

void RoomRenderer::DrawSimpleRoom(const SimpleRoom& room,
                                  const Theme& theme,
                                  const glm::mat4& view) {
  float height = room.height();
  float width = room.width();

  {
    // Front wall
    glm::mat4 model(1.f);
    model = glm::scale(model, glm::vec3(width, 1.0f, height));
    DrawWall(model, view, {height, width}, theme.front_appearance());
  }

  {
    // Floor wall
    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, -0.5 * kMaxDistance, -0.5 * height));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(width, 1.0f, kMaxDistance));
    DrawWall(model, view, {width, kMaxDistance}, theme.floor_appearance());
  }

  {
    // Left wall
    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(-0.5 * width, -0.5 * kMaxDistance, 0));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
    DrawWall(model, view, {kMaxDistance, height}, theme.side_appearance());
  }

  {
    // Right wall
    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0.5 * width, -0.5 * kMaxDistance, 0));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
    DrawWall(model, view, {kMaxDistance, height}, theme.side_appearance());
  }

  {
    // Top wall
    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(0, -0.5 * kMaxDistance, 0.5 * height));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(width, 1.0f, kMaxDistance));
    DrawWall(model, view, {width, kMaxDistance}, theme.roof_appearance());
  }
}

}  // namespace aim

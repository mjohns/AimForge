#pragma once

#include <filesystem>
#include <glm/mat4x4.hpp>

#include "aim/graphics/shader.h"
#include "aim/proto/scenario.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {

struct RoomRenderer {
 public:
  RoomRenderer(const std::filesystem::path& texture_folder);
  ~RoomRenderer();

  void SetProjection(const glm::mat4& projection);
  void Draw(const Room& room, const Theme& theme, const glm::mat4& view);

 private:
  void DrawSimpleRoom(const SimpleRoom& room, const Theme& theme, const glm::mat4& view);
  void DrawCircularRoom(const CircularRoom& room, const Theme& theme, const glm::mat4& view);

  unsigned int quad_vbo_;
  unsigned int quad_vao_;

  unsigned int circular_wall_vbo_;
  unsigned int circular_wall_vao_;
  unsigned int circular_wall_num_vertices_;

  unsigned int texture_;

  // Color a quad all with a single color.
  Shader simple_shader_;
  Shader pattern_shader_;
};

}  // namespace aim
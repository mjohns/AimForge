#pragma once

#include <filesystem>
#include <glm/mat4x4.hpp>

#include "aim/common/simple_types.h"
#include "aim/graphics/shader.h"
#include "aim/graphics/textures.h"
#include "aim/proto/scenario.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {

struct RoomRenderer {
 public:
  explicit RoomRenderer(TextureManager* texture_manager);
  ~RoomRenderer();

  void SetProjection(const glm::mat4& projection);
  void Draw(const Room& room, const Theme& theme, const glm::mat4& view);

 private:
  void DrawSimpleRoom(const SimpleRoom& room, const Theme& theme, const glm::mat4& view);
  void DrawCylinderRoom(const CylinderRoom& room, const Theme& theme, const glm::mat4& view);
  void DrawBarrelRoom(const BarrelRoom& room, const Theme& theme, const glm::mat4& view);

  void DrawWall(const glm::mat4& model,
                const glm::mat4& view,
                const Wall& wall,
                const WallAppearance& appearance,
                bool is_cylinder_wall = false);
  void DrawWallSolidColor(const glm::mat4& model,
                          const glm::mat4& view,
                          const glm::vec3& color,
                          bool is_cylinder_wall = false);

  unsigned int quad_vbo_;
  unsigned int quad_vao_;

  unsigned int cylinder_wall_vbo_;
  unsigned int cylinder_wall_vao_;
  unsigned int cylinder_wall_num_vertices_;

  Shader simple_shader_;
  Shader texture_shader_;

  TextureManager* texture_manager_;
};

}  // namespace aim
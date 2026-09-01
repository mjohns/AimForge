#pragma once

#include <vector>

#include "aim/common/simple_types.h"
#include "aim/core/target.h"
#include "aim/graphics/textures.h"
#include "aim/proto/scenario.pb.h"
#include "aim/proto/settings.pb.h"
#include "aim/proto/theme.pb.h"
#include "glm/mat4x4.hpp"  // IWYU pragma: keep
#include "glm/vec4.hpp"    // IWYU pragma: keep

namespace aim {

struct SolidColorInstanceData {
  glm::mat4 transform;
  glm::vec4 color;
};

struct SolidColorInstances {
  std::vector<SolidColorInstanceData> instances;

  // Instances will be packed in the following order and summing num_* equals the size of instances
  // vector.
  u32 num_spheres = 0;
  u32 num_cylinders = 0;
  u32 num_quads = 0;
  u32 num_cylinder_walls = 0;
  u32 num_circles = 0;

  u32 GetSpheresOffset() const {
    return 0;
  }

  u32 GetCylindersOffset() const {
    return num_spheres;
  }

  u32 GetQuadsOffset() const {
    return GetCylindersOffset() + num_cylinders;
  }

  u32 GetCylinderWallsOffset() const {
    return GetQuadsOffset() + num_quads;
  }

  u32 GetCirclesOffset() const {
    return GetCylinderWallsOffset() + num_cylinder_walls;
  }
};

struct TexScaleAndTransform {
  glm::vec4 tex_scale{};
  glm::mat4 transform{};
};

struct TextureWallDrawData {
  glm::mat4 transform;
  glm::vec4 color;
  bool is_cylinder = false;
  glm::vec2 tex_scale;
  Texture* texture;
};

struct SolidColorInstancedUniform {
  u32 instance_offset = 0;
};

struct DrawData {
  std::vector<TextureWallDrawData> texture_walls;

  std::vector<SolidColorInstanceData> solid_spheres;
  std::vector<SolidColorInstanceData> solid_cylinders;
  std::vector<SolidColorInstanceData> solid_quads;
  std::vector<SolidColorInstanceData> solid_cylinder_walls;
  std::vector<SolidColorInstanceData> solid_circles;

  void Clear() {
    texture_walls.clear();
    solid_spheres.clear();
    solid_cylinders.clear();
    solid_quads.clear();
    solid_cylinder_walls.clear();
    solid_circles.clear();
  }
};

// Turns the scenario scene description into normalized and sorted draw data that can be easily
// rendered.
void GetDrawDataForScenario(const glm::mat4& view_projection,
                            const Room& room,
                            bool draw_center,
                            const Theme& theme,
                            const HealthBarSettings& health_bar,
                            const std::vector<Target>& targets,
                            const LookAtInfo& look_at,
                            TextureManager& texture_manager,
                            DrawData* draw_data);

}  // namespace aim

#pragma once

#include <filesystem>
#include <vector>

#include "aim/core/target.h"
#include "aim/graphics/cylinder_renderer.h"
#include "aim/graphics/room_renderer.h"
#include "aim/graphics/sphere_renderer.h"
#include "aim/graphics/textures.h"
#include "aim/proto/scenario.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {

class Renderer {
 public:
  Renderer(const std::vector<std::filesystem::path>& texture_dirs)
      : room_renderer_(&texture_manager_), texture_manager_(texture_dirs) {}

  void SetProjection(const glm::mat4& projection);

  void DrawScenario(const Room& room,
                    const Theme& theme,
                    const std::vector<Target>& targets,
                    const glm::mat4& view);

  Renderer(const Renderer&) = delete;
  Renderer(Renderer&&) = default;
  Renderer& operator=(Renderer other) = delete;
  Renderer& operator=(Renderer&& other) = delete;

 private:
  SphereRenderer sphere_renderer_;
  CylinderRenderer cylinder_renderer_;
  RoomRenderer room_renderer_;
  TextureManager texture_manager_;
};

}  // namespace aim

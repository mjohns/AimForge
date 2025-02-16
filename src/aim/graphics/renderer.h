#pragma once

#include <filesystem>

#include "aim/core/target.h"
#include "aim/graphics/room_renderer.h"
#include "aim/graphics/sphere_renderer.h"
#include "aim/proto/scenario.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {

class Renderer {
 public:
  Renderer(const std::filesystem::path& texture_folder) : room_renderer_(texture_folder) {}
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
  RoomRenderer room_renderer_;
};

}  // namespace aim

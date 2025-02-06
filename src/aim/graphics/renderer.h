#pragma once

#include "aim/core/target.h"
#include "aim/graphics/room_renderer.h"
#include "aim/graphics/sphere_renderer.h"

namespace aim {

class Renderer {
 public:
  Renderer() {}
  void SetProjection(const glm::mat4& projection);

  void DrawTargets(const std::vector<Target>& targets, const glm::mat4& view);
  void DrawRoom(float height, float width, const glm::mat4& view);
  void DrawSimpleStaticRoom(float height,
                            float width,
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

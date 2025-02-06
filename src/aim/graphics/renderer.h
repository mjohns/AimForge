#pragma once

#include "aim/graphics/sphere_renderer.h"
#include "aim/graphics/room.h"
#include "aim/core/target.h"

namespace aim {

class Renderer {
 public:
  Renderer() {}
  void SetProjection(const glm::mat4& projection);
  void DrawTargets(const std::vector<Target>& targets, const glm::mat4& view);

  Renderer(const Renderer&) = delete;
  Renderer(Renderer&&) = default;
  Renderer& operator=(Renderer other) = delete;
  Renderer& operator=(Renderer&& other) = delete;

 private:
  SphereRenderer sphere_renderer_;
};

} // namespace aim

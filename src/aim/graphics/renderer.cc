#include "renderer.h"

namespace aim {

void Renderer::SetProjection(const glm::mat4& projection) {
  sphere_renderer_.SetProjection(projection);
  room_renderer_.SetProjection(projection);
}

void Renderer::DrawTargets(const std::vector<Target>& targets, const glm::mat4& view) {
  for (const Target& target : targets) {
    if (!target.hidden) {
      sphere_renderer_.Draw(view, {{target.position, target.radius}});
    }
  }
}

void Renderer::DrawRoom(float height, float width, const glm::mat4& view) {
  room_renderer_.Draw(height, width, view);
}

void Renderer::DrawSimpleStaticRoom(float height,
                                    float width,
                                    const std::vector<Target>& targets,
                                    const glm::mat4& view) {
  this->DrawRoom(height, width, view);
  this->DrawTargets(targets, view);
}

}  // namespace aim

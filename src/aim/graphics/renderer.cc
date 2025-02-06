#include "renderer.h"

namespace aim {

void Renderer::SetProjection(const glm::mat4& projection) {
  sphere_renderer_.SetProjection(projection);
}

void Renderer::DrawTargets(const std::vector<Target>& targets, const glm::mat4& view) {
  for (const Target& target : targets) {
    if (!target.hidden) {
      sphere_renderer_.Draw(view, {{target.position, target.radius}});
    }
  }
}

}  // namespace aim

#include "renderer.h"

namespace aim {

void Renderer::SetProjection(const glm::mat4& projection) {
  sphere_renderer_.SetProjection(projection);
  room_renderer_.SetProjection(projection);
}

void Renderer::DrawScenario(const Room& room,
                            const std::vector<Target>& targets,
                            const glm::mat4& view) {
  room_renderer_.Draw(room, view);
  for (const Target& target : targets) {
    if (!target.hidden) {
      sphere_renderer_.Draw(view, {{target.position, target.radius}});
    }
  }
}

}  // namespace aim

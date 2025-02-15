#include "renderer.h"

#include "aim/common/util.h"

namespace aim {

void Renderer::SetProjection(const glm::mat4& projection) {
  sphere_renderer_.SetProjection(projection);
  room_renderer_.SetProjection(projection);
}

void Renderer::DrawScenario(const Room& room,
                            const Theme& theme,
                            const std::vector<Target>& targets,
                            const glm::mat4& view) {
  room_renderer_.Draw(room, theme, view);
  for (const Target& target : targets) {
    if (!target.hidden) {
      sphere_renderer_.Draw(view,
                            theme.has_target_color() ? ToVec4(theme.target_color()) : glm::vec4(0),
                            {{target.position, target.radius}});
    }
  }
}

}  // namespace aim

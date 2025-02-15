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
  glm::vec3 target_color = theme.has_target_color() ? ToVec3(theme.target_color()) : glm::vec3(0);
  glm::vec3 ghost_target_color =
      theme.has_ghost_target_color() ? ToVec3(theme.ghost_target_color()) : glm::vec3(0.3);
  for (const Target& target : targets) {
    if (target.ShouldDraw()) {
      sphere_renderer_.Draw(view,
                            target.is_ghost ? ghost_target_color : target_color,
                            {{target.position, target.radius}});
    }
  }
}

}  // namespace aim

#include "renderer.h"

#include "aim/common/util.h"

namespace aim {

void Renderer::SetProjection(const glm::mat4& projection) {
  sphere_renderer_.SetProjection(projection);
  room_renderer_.SetProjection(projection);
  cylinder_renderer_.SetProjection(projection);
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
      glm::vec3 color = target.is_ghost ? ghost_target_color : target_color;
      if (target.is_pill) {
        Cylinder c;
        c.radius = target.radius;
        float cylinder_height = (target.height - target.radius) * 0.5;
        c.top_position = target.position + glm::vec3(0, 0, cylinder_height);
        c.bottom_position = target.position + glm::vec3(0, 0, cylinder_height * -1);
        cylinder_renderer_.Draw(view, color, {c});

        sphere_renderer_.Draw(
            view, color, {{c.top_position, target.radius}, {c.bottom_position, target.radius}});
      } else {
        sphere_renderer_.Draw(view, color, {{target.position, target.radius}});
      }
    }
  }
}

}  // namespace aim

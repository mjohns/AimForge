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
        c.up = target.pill_up;
        c.height = target.height - target.radius;
        c.position = target.position;
        cylinder_renderer_.Draw(ToVec3(room.camera_position()), view, color, {c});

        sphere_renderer_.Draw(view,
                              color,
                              {{c.position + c.up * (c.height * 0.5f), target.radius},
                               {c.position + c.up * (c.height * -0.5f), target.radius}});
      } else {
        sphere_renderer_.Draw(view, color, {{target.position, target.radius}});
      }
    }
  }
}

}  // namespace aim

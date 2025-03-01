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

  std::vector<Sphere> target_spheres;
  std::vector<Sphere> ghost_spheres;
  for (const Target& target : targets) {
    if (target.ShouldDraw()) {
      std::vector<Sphere>& spheres = target.is_ghost ? ghost_spheres : target_spheres;
      glm::vec3 color = target.is_ghost ? ghost_target_color : target_color;
      if (target.is_pill) {
        Cylinder c;
        c.radius = target.radius;
        c.up = target.pill_up;
        c.height = target.height - target.radius;
        c.position = target.position;
        cylinder_renderer_.Draw(ToVec3(room.camera_position()), view, color, {c});

        spheres.push_back({});
        Sphere& top = spheres.back();
        top.position = c.position + c.up * (c.height * 0.5f);
        top.radius = target.radius;

        spheres.push_back({});
        Sphere& bottom = spheres.back();
        bottom.position = c.position + c.up * (c.height * -0.5f);
        bottom.radius = target.radius;
      } else {
        spheres.push_back({});
        Sphere& s = spheres.back();
        s.position = target.position;
        s.radius = target.radius;
      }
    }
  }
  sphere_renderer_.Draw(view, target_color, target_spheres);
  sphere_renderer_.Draw(view, ghost_target_color, ghost_spheres);
}

}  // namespace aim

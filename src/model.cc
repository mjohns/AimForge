#include "model.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include "imgui.h"
#include "camera.h"

namespace aim {

ImVec2 GetScreenPosition(const glm::vec3& target,
                         const glm::mat4& transform,
                         const ScreenInfo& screen) {
  glm::vec4 position = {target.x, target.y, target.z, 1.0};
  glm::vec4 clip_space = transform * position;

  // Perform perspective division
  glm::vec3 ndc_space = glm::vec3(clip_space) / clip_space.w;

  // Convert NDC space (-1 to 1) to screen space (0 to
  // width/height)
  float screen_x = (ndc_space.x + 1.0f) * 0.5f * screen.width;
  float screen_y = (1.0f - ndc_space.y) * 0.5f * screen.height;

  return ImVec2(screen_x, screen.height - screen_y);
}

void Room::Draw(ImDrawList* draw_list, const glm::mat4& transform, const ScreenInfo& screen) {
  float max_x = wall_width * 0.5;
  float min_x = -1 * max_x;

  float max_y = wall_height * camera_height_percent;
  float min_y = max_y - wall_height;

  float wall_z = wall_distance * -1.0f;

  auto top_left = GetScreenPosition({min_x, max_y, wall_z}, transform, screen);
  auto top_right = GetScreenPosition({max_x, max_y, wall_z}, transform, screen);
  auto bottom_left = GetScreenPosition({min_x, min_y, wall_z}, transform, screen);
  auto bottom_right = GetScreenPosition({max_x, min_y, wall_z}, transform, screen);

  ImVec2 points[] = {top_left, top_right, bottom_right, bottom_left};
    draw_list->AddPolyline(points, 4, IM_COL32(238, 232, 213, 255), true, 5.f);

}

}  // namespace aim
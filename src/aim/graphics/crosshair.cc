#include "crosshair.h"

#include "aim/common/util.h"
#include "aim/proto/settings.pb.h"

namespace aim {

void DrawCrosshair(const Crosshair& crosshair, const ScreenInfo& screen, ImDrawList* draw_list) {
  if (!crosshair.has_dot()) {
    return;
  }
  float radius = crosshair.size();
  ImU32 circle_color = ToImCol32(crosshair.color());
  draw_list->AddCircleFilled(screen.center, radius, circle_color, 0);
  if (crosshair.dot().draw_outline()) {
    ImU32 outline_color = ToImCol32(crosshair.dot().outline_color());
    draw_list->AddCircle(screen.center, radius, outline_color, 0);
  }
}

}  // namespace aim
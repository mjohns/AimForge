#include "crosshair.h"

#include "aim/common/util.h"
#include "aim/fbs/settings_generated.h"

namespace aim {

void DrawCrosshair(const CrosshairT& crosshair, const ScreenInfo& screen, ImDrawList* draw_list) {
  DotCrosshairT* dot = crosshair.dot.get();
  if (dot == nullptr) {
    return;
  }
  float radius = dot->dot_size;
  ImU32 circle_color = ToImCol32(*dot->dot_color);
  draw_list->AddCircleFilled(screen.center, radius, circle_color, 0);
  if (dot->draw_outline) {
    ImU32 outline_color = ToImCol32(*dot->outline_color);
    draw_list->AddCircle(screen.center, radius, outline_color, 0);
  }
}

}  // namespace aim
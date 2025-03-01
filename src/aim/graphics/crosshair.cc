#include "crosshair.h"

#include "aim/common/util.h"
#include "aim/proto/settings.pb.h"

namespace aim {
namespace {

void DrawPlusWithLengths(float horizontal_length,
                         float vertical_length,
                         float thickness,
                         float rounding,
                         ImU32 color,
                         const ScreenInfo& screen,
                         ImDrawList* draw_list) {
  if (horizontal_length > 0) {
    ImVec2 horizontal_upper_left;
    ImVec2 horizontal_bottom_right;

    horizontal_upper_left.x = screen.center.x - vertical_length;
    horizontal_bottom_right.x = screen.center.x + vertical_length;

    horizontal_upper_left.y = screen.center.y - thickness;
    horizontal_bottom_right.y = screen.center.y + thickness;

    draw_list->AddRectFilled(horizontal_upper_left, horizontal_bottom_right, color, rounding);
  }

  if (vertical_length > 0) {
    ImVec2 vertical_upper_left;
    ImVec2 vertical_bottom_right;

    vertical_upper_left.x = screen.center.x - thickness;
    vertical_bottom_right.x = screen.center.x + thickness;

    vertical_upper_left.y = screen.center.y - vertical_length;
    vertical_bottom_right.y = screen.center.y + vertical_length;

    draw_list->AddRectFilled(vertical_upper_left, vertical_bottom_right, color, rounding);
  }
}

}  // namespace

void DrawCrosshair(const Crosshair& crosshair,
                   float crosshair_size,
                   const Theme& theme,
                   const ScreenInfo& screen,
                   ImDrawList* draw_list) {
  ImU32 main_color = ToImCol32(theme.crosshair().color());
  ImU32 outline_color = ToImCol32(theme.crosshair().outline_color());
  if (crosshair.has_dot()) {
    float radius = crosshair_size / 4.0f;
    draw_list->AddCircleFilled(screen.center, radius, main_color, 0);
    if (crosshair.dot().draw_outline()) {
      draw_list->AddCircle(screen.center, radius, outline_color, 0);
    }
  }

  if (crosshair.has_plus()) {
    const PlusCrosshair& plus = crosshair.plus();
    float base_length = crosshair_size * 0.55f;
    float thickness = base_length * 0.2 * (plus.has_thickness() ? plus.thickness() : 1.0f);

    float horizontal_length = base_length * 0.5;
    if (plus.has_horizontal_size()) {
      horizontal_length *= plus.horizontal_size();
    }
    float vertical_length = base_length * 0.5;
    if (plus.has_vertical_size()) {
      vertical_length *= plus.vertical_size();
    }

    if (plus.outline_thickness() > 0) {
      DrawPlusWithLengths(horizontal_length,
                          vertical_length,
                          thickness,
                          plus.rounding(),
                          outline_color,
                          screen,
                          draw_list);
      float outline_size = plus.outline_thickness();
      horizontal_length -= outline_size;
      vertical_length -= outline_size;
      thickness -= outline_size;
    }

    DrawPlusWithLengths(horizontal_length,
                        vertical_length,
                        thickness,
                        plus.rounding(),
                        main_color,
                        screen,
                        draw_list);
  }
}

}  // namespace aim
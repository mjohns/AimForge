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
                         const ImVec2& center,
                         ImDrawList* draw_list) {
  if (horizontal_length > 0) {
    ImVec2 horizontal_upper_left;
    ImVec2 horizontal_bottom_right;

    horizontal_upper_left.x = center.x - horizontal_length;
    horizontal_bottom_right.x = center.x + horizontal_length;

    horizontal_upper_left.y = center.y - thickness;
    horizontal_bottom_right.y = center.y + thickness;

    draw_list->AddRectFilled(horizontal_upper_left, horizontal_bottom_right, color, rounding);
  }

  if (vertical_length > 0) {
    ImVec2 vertical_upper_left;
    ImVec2 vertical_bottom_right;

    vertical_upper_left.x = center.x - thickness;
    vertical_bottom_right.x = center.x + thickness;

    vertical_upper_left.y = center.y - vertical_length;
    vertical_bottom_right.y = center.y + vertical_length;

    draw_list->AddRectFilled(vertical_upper_left, vertical_bottom_right, color, rounding);
  }
}

}  // namespace

void DrawCrosshairLayer(const CrosshairLayer& layer,
                        float crosshair_size,
                        const Theme& theme,
                        const ImVec2& center) {
  StoredRgb main_rgb = ToStoredRgb(theme.crosshair().color());
  if (layer.has_override_color()) {
    main_rgb = ToStoredRgb(layer.override_color());
  }
  StoredRgb outline_rgb = ToStoredRgb(theme.crosshair().outline_color());
  if (layer.has_override_outline_color()) {
    outline_rgb = ToStoredRgb(layer.override_outline_color());
  }

  u8 alpha = 255;
  if (layer.alpha() > 0) {
    alpha = std::min<u8>(alpha, alpha * layer.alpha());
  }
  ImU32 main_color = ToImCol32(main_rgb, alpha);
  ImU32 outline_color = ToImCol32(outline_rgb, alpha);

  if (layer.scale() > 0) {
    crosshair_size *= layer.scale();
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  if (layer.has_dot()) {
    float radius = crosshair_size / 4.0f;
    draw_list->AddCircleFilled(center, radius, main_color, 0);
    if (layer.dot().outline_thickness() > 0) {
      draw_list->AddCircle(center, radius, outline_color, 0, layer.dot().outline_thickness());
    }
    return;
  }

  if (layer.has_plus()) {
    const PlusCrosshair& plus = layer.plus();
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
                          center,
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
                        center,
                        draw_list);
    return;
  }

  // Handle other crosshair types.
}

void DrawCrosshair(const Crosshair& crosshair,
                   float crosshair_size,
                   const Theme& theme,
                   const ImVec2& center) {
  for (auto& layer : crosshair.layers()) {
    DrawCrosshairLayer(layer, crosshair_size, theme, center);
  }
}

}  // namespace aim
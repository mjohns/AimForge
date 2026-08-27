#include "crosshair_renderer.h"

#include "aim/common/util.h"

namespace aim {
namespace {

void DrawPlusWithLengths(float horizontal_length,
                         float vertical_length,
                         float thickness,
                         float rounding,
                         float horizontal_gap_length,
                         float vertical_gap_length,
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

    if (horizontal_gap_length > 0) {
      // Draw left side.
      ImVec2 center_bottom_left;
      center_bottom_left.x = center.x - horizontal_gap_length;
      center_bottom_left.y = horizontal_bottom_right.y;
      horizontal_upper_left.x -= horizontal_gap_length;
      draw_list->AddRectFilled(horizontal_upper_left, center_bottom_left, color, rounding);

      // Draw right side.
      ImVec2 center_upper_right;
      center_upper_right.x = center.x + horizontal_gap_length;
      center_upper_right.y = horizontal_upper_left.y;
      horizontal_bottom_right.x += horizontal_gap_length;
      draw_list->AddRectFilled(center_upper_right, horizontal_bottom_right, color, rounding);
    } else {
      draw_list->AddRectFilled(horizontal_upper_left, horizontal_bottom_right, color, rounding);
    }
  }

  if (vertical_length > 0) {
    ImVec2 vertical_upper_left;
    ImVec2 vertical_bottom_right;

    vertical_upper_left.x = center.x - thickness;
    vertical_bottom_right.x = center.x + thickness;

    vertical_upper_left.y = center.y - vertical_length;
    vertical_bottom_right.y = center.y + vertical_length;

    if (vertical_gap_length > 0) {
      // Draw top.
      ImVec2 upper_bottom_right;
      upper_bottom_right.x = vertical_bottom_right.x;
      upper_bottom_right.y = center.y - vertical_gap_length;
      vertical_upper_left.y -= vertical_gap_length;
      draw_list->AddRectFilled(vertical_upper_left, upper_bottom_right, color, rounding);

      // Draw bottom.
      ImVec2 bottom_upper_left;
      bottom_upper_left.x = vertical_upper_left.x;
      bottom_upper_left.y = center.y + vertical_gap_length;
      vertical_bottom_right.y += vertical_gap_length;
      draw_list->AddRectFilled(bottom_upper_left, vertical_bottom_right, color, rounding);
    } else {
      draw_list->AddRectFilled(vertical_upper_left, vertical_bottom_right, color, rounding);
    }
  }
}

}  // namespace

CrosshairRenderer::CrosshairRenderer(const std::filesystem::path& crosshair_dir,
                                     SDL_GPUDevice* gpu_device)
    : texture_manager_({crosshair_dir}, gpu_device) {}

void CrosshairRenderer::Draw(const Crosshair& crosshair,
                             float crosshair_size,
                             const Theme& theme,
                             const ImVec2& center) {
  for (auto& layer : crosshair.layers()) {
    DrawLayer(layer, crosshair_size, theme, center);
  }
}

void CrosshairRenderer::DrawLayer(const CrosshairLayer& layer,
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

  glm::vec3 main_color3 = ToVec3(main_rgb);
  ImVec4 main_color4(main_color3.r, main_color3.g, main_color3.b, alpha / 255.0f);

  if (layer.scale() > 0) {
    crosshair_size *= layer.scale();
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  // draw_list->Flags

  if (layer.has_dot()) {
    float radius = crosshair_size / 4.0f;
    draw_list->AddCircleFilled(center, radius, main_color, 0);
    if (layer.dot().outline_thickness() > 0) {
      draw_list->AddCircle(center, radius, outline_color, 0, layer.dot().outline_thickness());
    }
    return;
  }

  if (layer.has_circle()) {
    float radius = crosshair_size / 4.0f;
    float thickness = FirstGreaterThanZero(layer.circle().thickness(), 1.5);
    draw_list->AddCircle(center,
                         radius,
                         layer.circle().use_outline_color() ? outline_color : main_color,
                         0,
                         thickness);
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
    float vertical_gap_length = (plus.vertical_gap_size() * base_length) / 2.0;
    float horizontal_gap_length = (plus.horizontal_gap_size() * base_length) / 2.0;

    if (plus.outline_thickness() > 0) {
      DrawPlusWithLengths(horizontal_length,
                          vertical_length,
                          thickness,
                          plus.rounding(),
                          horizontal_gap_length,
                          vertical_gap_length,
                          outline_color,
                          center,
                          draw_list);
      float outline_size = plus.outline_thickness();
      horizontal_length -= outline_size;
      vertical_length -= outline_size;
      thickness -= outline_size;
      if (horizontal_gap_length > 0) {
        horizontal_gap_length += outline_size;
        horizontal_length -= outline_size;
      }
      if (vertical_gap_length > 0) {
        vertical_gap_length += outline_size;
        vertical_length -= outline_size;
      }
    }

    DrawPlusWithLengths(horizontal_length,
                        vertical_length,
                        thickness,
                        plus.rounding(),
                        horizontal_gap_length,
                        vertical_gap_length,
                        main_color,
                        center,
                        draw_list);
    return;
  }

  if (layer.has_diamond()) {
    const DiamondCrosshair& d = layer.diamond();
    float base_length = crosshair_size * 0.55f;

    float horizontal_length = base_length * 0.5;
    if (d.has_horizontal_size()) {
      horizontal_length *= d.horizontal_size();
    }
    float vertical_length = base_length * 0.5;
    if (d.has_vertical_size()) {
      vertical_length *= d.vertical_size();
    }

    ImVec2 left(center.x - horizontal_length, center.y);
    ImVec2 right(center.x + horizontal_length, center.y);
    ImVec2 up(center.x, center.y - vertical_length);
    ImVec2 down(center.x, center.y + vertical_length);
    draw_list->AddQuadFilled(up, right, down, left, main_color);

    if (d.outline_thickness() > 0) {
      draw_list->AddQuad(up, right, down, left, outline_color, d.outline_thickness());
    }

    return;
  }

  if (layer.has_image()) {
    Texture* texture = texture_manager_.GetTexture(layer.image().file_name());
    if (texture != nullptr) {
      float mult = (crosshair_size / 15.0f);
      float width = texture->width() * mult;
      float height = texture->height() * mult;
      ImVec2 old_pos = ImGui::GetCursorScreenPos();
      ImGui::SetCursorScreenPos(ImVec2(center.x - (width / 2.0), center.y - (height / 2.0)));
      ImGui::ImageWithBg(texture->GetImTextureId(),
                         ImVec2(width, height),
                         ImVec2(0.0f, 0.0f),
                         ImVec2(1.0f, 1.0f),
                         ImVec4(0, 0, 0, 0),
                         main_color4);
      ImGui::SetCursorScreenPos(old_pos);
    }
    return;
  }

  // Handle other crosshair types.
}

}  // namespace aim

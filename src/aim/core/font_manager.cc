#include "font_manager.h"

#include "aim/common/log.h"

namespace aim {
namespace {}  // namespace

int FontManager::large_font_size() {
  return 38;
}

int FontManager::default_font_size() {
  return 22;
}

int FontManager::medium_font_size() {
  return 30;
}

bool FontManager::LoadFonts() {
  auto font_path = fonts_path_ / "Roboto-Regular.ttf";
  auto bold_font_path = fonts_path_ / "Roboto-Bold.ttf";
  auto material_icons_path = fonts_path_ / "MaterialIcons-Regular.ttf";
  // auto material_icons_path = fonts_path_ / "MaterialIconsOutlined-Regular.otf";

  ImGuiIO& io = ImGui::GetIO();
  default_font_ = io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), default_font_size());
  if (default_font_ == nullptr) {
    Logger::get()->error("Unable to load default font from: {}", font_path.string());
    return false;
  }

  // See https://github.com/ocornut/imgui/issues/3247
  static const ImWchar icons_ranges[] = {0xE005, 0xFFFF, 0};
  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.PixelSnapH = true;
  icons_config.GlyphOffset.y = 4;

  material_icons_font_ = io.Fonts->AddFontFromFileTTF(
      material_icons_path.string().c_str(), default_font_size(), &icons_config, icons_ranges);
  if (material_icons_font_ == nullptr) {
    Logger::get()->error("Unable to load material icons font from: {}",
                         material_icons_path.string());
    return false;
  }

  large_font_ = io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), large_font_size());
  if (large_font_ == nullptr) {
    Logger::get()->error("Unable to load large font from: {}", font_path.string());
    return false;
  }

  medium_font_ = io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), medium_font_size());
  if (medium_font_ == nullptr) {
    Logger::get()->error("Unable to load medium font from: {}", font_path.string());
    return false;
  }
  icons_config.GlyphOffset.y = 6;
  io.Fonts->AddFontFromFileTTF(
      material_icons_path.string().c_str(), medium_font_size(), &icons_config, icons_ranges);

  default_bold_font_ =
      io.Fonts->AddFontFromFileTTF(bold_font_path.string().c_str(), default_font_size());
  if (default_bold_font_ == nullptr) {
    Logger::get()->error("Unable to load default bold font from: {}", bold_font_path.string());
    return false;
  }

  large_bold_font_ =
      io.Fonts->AddFontFromFileTTF(bold_font_path.string().c_str(), large_font_size());
  if (large_bold_font_ == nullptr) {
    Logger::get()->error("Unable to load large bold font from: {}", bold_font_path.string());
    return false;
  }

  medium_bold_font_ =
      io.Fonts->AddFontFromFileTTF(bold_font_path.string().c_str(), medium_font_size());
  if (medium_bold_font_ == nullptr) {
    Logger::get()->error("Unable to load medium bold font from: {}", bold_font_path.string());
    return false;
  }

  return true;
}

}  // namespace aim

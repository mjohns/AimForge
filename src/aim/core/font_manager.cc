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
bool FontManager::LoadFonts() {
  auto font_path = fonts_path_ / "Roboto-Regular.ttf";
  auto bold_font_path = fonts_path_ / "Roboto-Bold.ttf";

  ImGuiIO& io = ImGui::GetIO();
  default_font_ = io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), default_font_size());
  if (default_font_ == nullptr) {
    Logger::get()->error("Unable to load default font from: {}", font_path.string());
    return false;
  }

  large_font_ = io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), large_font_size());
  if (large_font_ == nullptr) {
    Logger::get()->error("Unable to load large font from: {}", font_path.string());
    return false;
  }

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

  return true;
}

}  // namespace aim

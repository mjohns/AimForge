#pragma once

#include <imgui.h>

#include <filesystem>
#include <optional>

namespace aim {

class ScopedFont {
 public:
  explicit ScopedFont(ImFont* font, std::optional<ImU32> color) : font_(font), font_color_(color) {
    if (font_ != nullptr) {
      ImGui::PushFont(font_);
    }
    // ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
  }

  ~ScopedFont() {
    if (font_ != nullptr) {
      ImGui::PopFont();
    }
  }

  ScopedFont(const ScopedFont&) = delete;
  ScopedFont(ScopedFont&&) = default;
  ScopedFont& operator=(ScopedFont other) = delete;
  ScopedFont& operator=(ScopedFont&& other) = delete;

 private:
  ImFont* font_ = nullptr;
  std::optional<ImU32> font_color_;
};

class FontManager {
 public:
  explicit FontManager(const std::filesystem::path& fonts_path) : fonts_path_(fonts_path) {}

  bool LoadFonts();

  int large_font_size();
  int default_font_size();

  ScopedFont UseDefault(std::optional<ImU32> color = {}) {
    return ScopedFont(default_font_, color);
  }

  ScopedFont UseDefaultBold(std::optional<ImU32> color = {}) {
    return ScopedFont(default_bold_font_, color);
  }

  ScopedFont UseLarge(std::optional<ImU32> color = {}) {
    return ScopedFont(large_font_, color);
  }

  ScopedFont UseLargeBold(std::optional<ImU32> color = {}) {
    return ScopedFont(large_bold_font_, color);
  }

  FontManager(const FontManager&) = delete;
  FontManager(FontManager&&) = default;
  FontManager& operator=(FontManager other) = delete;
  FontManager& operator=(FontManager&& other) = delete;

 private:
  std::filesystem::path fonts_path_;

  ImFont* default_font_ = nullptr;
  ImFont* default_bold_font_ = nullptr;
  ImFont* large_font_ = nullptr;
  ImFont* large_bold_font_ = nullptr;
};

}  // namespace aim

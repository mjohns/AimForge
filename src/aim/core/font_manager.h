#pragma once

#include <imgui.h>

#include <filesystem>
#include <optional>

namespace aim {

class ScopedFont {
 public:
  explicit ScopedFont(ImFont* font) : font_(font) {
    if (font_ != nullptr) {
      ImGui::PushFont(font_);
    }
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
};

class FontManager {
 public:
  explicit FontManager(const std::filesystem::path& fonts_path) : fonts_path_(fonts_path) {}

  bool LoadFonts();

  int large_font_size();
  int medium_font_size();
  int default_font_size();

  ScopedFont UseDefault() {
    return ScopedFont(default_font_);
  }

  ScopedFont UseDefaultBold() {
    return ScopedFont(default_bold_font_);
  }

  ScopedFont UseLarge() {
    return ScopedFont(large_font_);
  }

  ScopedFont UseLargeBold() {
    return ScopedFont(large_bold_font_);
  }

  ScopedFont UseMedium() {
    return ScopedFont(medium_font_);
  }

  ScopedFont UseMediumBold() {
    return ScopedFont(medium_bold_font_);
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
  ImFont* medium_font_ = nullptr;
  ImFont* medium_bold_font_ = nullptr;
};

}  // namespace aim

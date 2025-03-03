#pragma once

#include <imgui.h>

#include <format>
#include <string>

namespace aim {

class ScopedStyleColor {
 public:
  ScopedStyleColor(ImGuiCol idx, ImU32 col) {
    ImGui::PushStyleColor(idx, col);
  }

  static ScopedStyleColor Text(ImU32 col) {
    return ScopedStyleColor(ImGuiCol_Text, col);
  }

  ~ScopedStyleColor() {
    End();
  }

  void End() {
    if (!closed_) {
      closed_ = true;
      ImGui::PopStyleColor();
    }
  }

 private:
  ImGuiCol idx_;
  ImU32 col_;
  bool closed_ = false;
};

}  // namespace aim

namespace ImGui {

// ImGui::Text but taking same arguments as std::format to product the text.
template <class... _Types>
static void TextFmt(const std::format_string<_Types...> fmt, _Types&&... args) {
  std::string message = std::format(fmt, std::forward<_Types>(args)...);
  Text(message.c_str());
}

static void Text(const std::string& val) {
  Text("%s", val.c_str());
}

}  // namespace ImGui

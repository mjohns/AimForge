#pragma once

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <string>

namespace ImGui {

// ImGui::Text but taking same arguments as std::format to product the text.
template <class... _Types>
static void TextFmt(const std::format_string<_Types...> fmt, _Types&&... args) {
  std::string message = std::format(fmt, std::forward<_Types>(args)...);
  Text("%s", message.c_str());
}

static void Text(const std::string& val) {
  Text("%s", val.c_str());
}

struct IdGuard {
  IdGuard(std::string id) {
    ImGui::PushID(id.c_str());
  }

  IdGuard(std::string prefix, int num) {
    ImGui::PushID(std::format("{}{}", prefix, num).c_str());
  }

  IdGuard(int id) {
    ImGui::PushID(id);
  }

  ~IdGuard() {
    Pop();
  }

  void Pop() {
    if (!closed) {
      ImGui::PopID();
      closed = true;
    }
  }

  IdGuard(const IdGuard&) = delete;
  IdGuard& operator=(const IdGuard&) = delete;

 private:
  bool closed = false;
};

struct LoopId {
  LoopId() {}

  IdGuard Get() {
    return IdGuard(++i);
  }

  int i = -1;
};

}  // namespace ImGui

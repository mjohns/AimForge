#pragma once

#include <imgui.h>
#include <string>

namespace ImGui {

static void Text(const std::string& val) {
  Text(val.c_str());
}

} // namespace ImGui

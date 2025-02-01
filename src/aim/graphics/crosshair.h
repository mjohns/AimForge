#pragma once

#include <imgui.h>

#include "aim/common/simple_types.h"
#include "aim/fbs/settings_generated.h"

namespace aim {

void DrawCrosshair(const CrosshairT& crosshair, const ScreenInfo& screen, ImDrawList* draw_list);

}  // namespace aim
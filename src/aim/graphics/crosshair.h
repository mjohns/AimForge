#pragma once

#include <imgui.h>

#include "aim/common/simple_types.h"
#include "aim/proto/settings.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {

void DrawCrosshair(const Crosshair& crosshair,
                   const Theme& theme,
                   const ScreenInfo& screen,
                   ImDrawList* draw_list);

}  // namespace aim
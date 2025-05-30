#pragma once

#include <imgui.h>

#include "aim/proto/settings.pb.h"
#include "aim/proto/theme.pb.h"

namespace aim {

void DrawCrosshair(const Crosshair& crosshair,
                   float crosshair_size,
                   const Theme& theme,
                   const ImVec2& center);

void DrawCrosshairLayer(const CrosshairLayer& layer,
                        float crosshair_size,
                        const Theme& theme,
                        const ImVec2& center);

}  // namespace aim
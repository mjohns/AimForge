#pragma once

#include "implot.h"

namespace ImPlot {

bool IsPointNearMouse(ImPlotPoint mouse_pos,
                      double point_x,
                      double point_y,
                      float pixel_threshold = 10.0f);

}  // namespace ImPlot

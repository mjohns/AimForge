#pragma once

#include "implot.h"

namespace ImPlot {

// Gets the amount of units in the plots coordinate system that corresponds to the provided pixel
// amount.
ImPlotPoint GetPlotDistanceFromPixels(float pixels);

bool IsPointNearMouse(ImPlotPoint mouse_pos,
                      double point_x,
                      double point_y,
                      float pixel_threshold = 10.0f);

}  // namespace ImPlot

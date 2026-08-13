#include "implot_ext.h"

#include <cmath>

namespace ImPlot {

ImPlotPoint GetPlotDistanceFromPixels(float pixels) {
  ImPlotPoint start = ImPlot::PixelsToPlot(0, 0);
  ImPlotPoint end = ImPlot::PixelsToPlot(pixels, pixels);

  ImPlotPoint result;
  result.x = std::abs(end.x - start.x);
  result.y = std::abs(end.y - start.y);
  return result;
}

bool IsPointNearMouse(ImPlotPoint mouse_pos,
                      double point_x,
                      double point_y,
                      float pixel_threshold) {
  ImPlotPoint threshold = GetPlotDistanceFromPixels(pixel_threshold);
  double dx = std::abs(mouse_pos.x - point_x);
  double dy = std::abs(mouse_pos.y - point_y);
  return dx < std::abs(threshold.x) && dy < std::abs(threshold.y);
}

}  // namespace ImPlot

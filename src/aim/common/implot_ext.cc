#include "implot_ext.h"

#include <cmath>

namespace ImPlot {

ImPlotPoint GetPlotDistanceFromPixels(float pixels) {
  ImPlotPoint start = ImPlot::PixelsToPlot(0, 0);
  ImPlotPoint end = ImPlot::PixelsToPlot(1, 1);

  ImPlotPoint result;
  result.x = abs(end.x - start.x) * pixels;
  result.y = abs(end.y - start.y) * pixels;
  return result;
}

bool IsPointNearMouse(ImPlotPoint mouse_pos,
                      double point_x,
                      double point_y,
                      float pixel_threshold) {
  ImPlotPoint threshold = GetPlotDistanceFromPixels(pixel_threshold);
  double dx = abs(mouse_pos.x - point_x);
  double dy = abs(mouse_pos.y - point_y);
  return dx < abs(threshold.x) && dy < abs(threshold.y);
}

}  // namespace ImPlot

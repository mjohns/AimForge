#include "implot_ext.h"

#include <cmath>

namespace ImPlot {

bool IsPointNearMouse(ImPlotPoint mouse_pos,
                      double point_x,
                      double point_y,
                      float pixel_threshold) {
  ImPlotPoint start = ImPlot::PixelsToPlot(0, 0);
  ImPlotPoint end = ImPlot::PixelsToPlot(1, 1);

  // Amount of distance in plot coordinate system to move 1 pixel.
  float unit_x = abs(end.x - start.x);
  float unit_y = abs(end.y - start.y);

  float threshold_x = unit_x * pixel_threshold;
  float threshold_y = unit_y * pixel_threshold;

  double dx = abs(mouse_pos.x - point_x);
  double dy = abs(mouse_pos.y - point_y);

  return dx < abs(threshold_x) && dy < abs(threshold_y);
}

}  // namespace ImPlot

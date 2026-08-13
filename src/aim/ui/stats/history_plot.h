#pragma once

#include <string>

#include "aim/core/stats_manager.h"

namespace aim {

void DrawHistoryPlot(const std::string& id,
                     const StatsDetails& details,
                     int max_to_show,
                     double score_target);

}  // namespace aim

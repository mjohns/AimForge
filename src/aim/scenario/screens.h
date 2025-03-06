#pragma once

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"
#include "aim/proto/replay.pb.h"
#include "aim/scenario/scenario.h"
#include "aim/ui/ui_screen.h"

namespace aim {

class StatsScreen {
 public:
  StatsScreen(std::string scenario_id, i64 stats_id, Application* app, FrameTimes worst_times);

  NavigationEvent Run(Replay* replay);

 private:
  std::string scenario_id_;
  i64 stats_id_;
  Application* app_;
  FrameTimes worst_times_;
};

enum class QuickSettingsType { DEFAULT, METRONOME };

std::unique_ptr<UiScreen> CreateQuickSettingsScreen(const std::string& scenario_id,
                                                    QuickSettingsType type,
                                                    Application* app);

}  // namespace aim

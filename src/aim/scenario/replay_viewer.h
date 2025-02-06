#pragma once

#include "aim/core/application.h"
#include "aim/fbs/replay_generated.h"
#include "aim/fbs/settings_generated.h"

namespace aim {

class ReplayViewer {
 public:
  bool PlayReplay(const StaticReplayT& replay, const CrosshairT& crosshair, Application* app);
};

}  // namespace aim

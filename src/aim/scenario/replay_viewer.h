#pragma once

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"
#include "aim/fbs/replay_generated.h"
#include "aim/fbs/settings_generated.h"

namespace aim {

class ReplayViewer {
 public:
  NavigationEvent PlayReplay(const StaticReplayT& replay,
                             const CrosshairT& crosshair,
                             Application* app);
};

}  // namespace aim

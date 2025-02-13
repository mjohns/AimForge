#pragma once

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"
#include "aim/proto/replay.pb.h"
#include "aim/proto/settings.pb.h"

namespace aim {

class ReplayViewer {
 public:
  NavigationEvent PlayReplay(const Replay& replay, const Crosshair& crosshair, Application* app);
};

}  // namespace aim

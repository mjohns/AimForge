#pragma once

#include "aim/core/application.h"
#include "aim/proto/settings.pb.h"
#include "aim/proto/theme.pb.h"
#include "aim/scenario/replay.h"

namespace aim {

class ReplayViewer {
 public:
  void PlayReplay(const ReplayV2& replay, Application* app);
};

}  // namespace aim

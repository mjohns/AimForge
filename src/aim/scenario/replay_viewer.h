#pragma once

#include <memory>

#include "aim/core/screen.h"
#include "aim/scenario/replay.h"

namespace aim {

class Application;

std::unique_ptr<Screen> CreateReplayViewerScreen(std::shared_ptr<Replay> replay, Application* app);

}  // namespace aim

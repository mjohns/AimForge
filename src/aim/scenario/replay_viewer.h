#pragma once

#include <memory>

#include "aim/core/screen.h"

namespace aim {

class Application;
// Not forward declaring this messes up the build in windows for some reason.
class Replay;

std::unique_ptr<Screen> CreateReplayViewerScreen(std::shared_ptr<Replay> replay, Application* app);

}  // namespace aim

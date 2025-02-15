#pragma once

#include "aim/core/application.h"
#include "aim/core/navigation_event.h"

namespace aim {

class SettingsScreen {
 public:
  NavigationEvent Run(Application* app);
};

class QuickSettingsScreen {
 public:
  NavigationEvent Run(Application* app);
};

}  // namespace aim

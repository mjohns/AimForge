#pragma once

#include <memory>

#include "aim/core/application.h"

namespace aim {

std::shared_ptr<Screen> CreateHomeScreen(Application* app);

}  // namespace aim

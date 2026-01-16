#pragma once

#include <memory>

#include "aim/ui/ui_screen.h"

namespace aim {

class BundleUiComponent {
 public:
  virtual ~BundleUiComponent() {}

  virtual void Show() = 0;
};

std::unique_ptr<BundleUiComponent> CreateBundleUiComponent(UiScreen* screen);

}  // namespace aim

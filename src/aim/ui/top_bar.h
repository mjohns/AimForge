#pragma once

#include <memory>

namespace aim {

class TopBar {
 public:
  virtual ~TopBar() {}

  virtual void Draw() = 0;
};

std::unique_ptr<TopBar> CreateTopBar();

}  // namespace aim

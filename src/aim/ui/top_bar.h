#pragma once

#include <memory>

namespace aim {

class TopBar {
 public:
  ~TopBar() {}

  virtual void Draw() = 0;
};

std::unique_ptr<TopBar> CreateTopBar();

}  // namespace aim

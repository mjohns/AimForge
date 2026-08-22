#pragma once

#include <memory>

namespace aim {


class GuidesComponent {
 public:
  virtual ~GuidesComponent() {}

  virtual void Show() = 0;
};

std::unique_ptr<GuidesComponent> CreateGuidesComponent();

}  // namespace aim

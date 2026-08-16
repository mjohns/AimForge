#pragma once

#include <memory>

namespace aim {

class Application;

class ScenariosComponent {
 public:
  virtual ~ScenariosComponent() {}

  virtual void Show() = 0;

  virtual void Reload() = 0;
};

// Component handling UI for the scenarios tab on the home screen
std::unique_ptr<ScenariosComponent> CreateScenariosComponent(Application& app);

}  // namespace aim

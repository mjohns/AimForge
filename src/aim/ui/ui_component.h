#pragma once

#include <memory>
#include <string>

#include "aim/common/imgui_ext.h"
#include "aim/core/application.h"

namespace aim {

class UiComponent {
 protected:
  explicit UiComponent(Application* app) : app_(app), component_id_(app->GetNextComponentId()) {}
  virtual ~UiComponent() {}

  ImGui::IdGuard GetComponentIdGuard() {
    return ImGui::IdGuard(std::format("Component{}", component_id_));
  }

  Application* app_;
  u64 component_id_;
};

}  // namespace aim

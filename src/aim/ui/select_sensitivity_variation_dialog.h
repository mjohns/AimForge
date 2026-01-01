#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aim/ui/ui_screen.h"

namespace aim {

class SelectSensitivityVariationDialog {
 public:
  explicit SelectSensitivityVariationDialog(const std::string& id) : id_(id) {}

  void NotifyOpen(std::optional<float> current_cm_per_360) {
    do_open_ = true;
    if (current_cm_per_360) {
      input_cm_per_360_ = *current_cm_per_360;
    }
  }

  bool Draw(std::optional<float>* selected_cm_per_360);

 private:
  bool do_open_ = false;
  bool draw_popup_ = false;

  std::string id_;
  float input_cm_per_360_ = 35;
};

}  // namespace aim

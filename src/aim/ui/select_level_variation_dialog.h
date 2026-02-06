#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aim/ui/ui_screen.h"

namespace aim {

class SelectLevelVariationDialog {
 public:
  explicit SelectLevelVariationDialog(const std::string& id) : id_(id) {}

  void NotifyOpen(std::optional<float> current_level) {
    do_open_ = true;
    input_level_ = current_level;
  }

  bool Draw(std::optional<float>* selected_level);

 private:
  bool do_open_ = false;
  bool draw_popup_ = false;

  std::string id_;
  std::optional<float> input_level_;
};

}  // namespace aim

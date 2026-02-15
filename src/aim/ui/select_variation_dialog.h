#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "aim/ui/ui_screen.h"

namespace aim {

class SelectVariationDialog {
 public:
  static SelectVariationDialog ForPlaylists(const std::string& id) {
    return SelectVariationDialog(id, /*is_playlist=*/true);
  }
  static SelectVariationDialog ForScenarios(const std::string& id) {
    return SelectVariationDialog(id, /*is_playlist=*/false);
  }

  SelectVariationDialog(const std::string& id, bool is_playlist)
      : popup_(id), is_playlist_(is_playlist), id_(id) {}

  void NotifyOpen(const std::string& current_name) {
    name_info_ =
        is_playlist_ ? GetPlaylistNameInfo(current_name) : GetScenarioNameInfo(current_name);
    popup_.Open();
  }

  bool Draw(std::string* updated_name);

 private:
  bool is_playlist_;
  ImGui::Popup popup_;
  std::string id_;
  NameInfo name_info_;
};

}  // namespace aim

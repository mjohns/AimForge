#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aim/core/playlist_manager.h"
#include "aim/ui/ui_screen.h"

namespace aim {

class CopyPlaylistDialog {
 public:
  explicit CopyPlaylistDialog(const std::string& id) : id_(id) {}

  void NotifyOpen(const Playlist& source) {
    open_ = true;
    source_ = source;
  }

  bool Draw(Application& app);

 private:
  bool open_ = false;

  bool deep_copy_ = false;
  bool as_references_ = false;
  std::string remove_prefix_;
  std::string add_prefix_;

  std::vector<std::string> bundle_names_;

  ResourceName new_name_;

  std::string id_;
  std::optional<Playlist> source_;
};

}  // namespace aim

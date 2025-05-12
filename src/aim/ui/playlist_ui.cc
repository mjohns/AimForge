#include "playlist_ui.h"

#include <imgui.h>

#include "aim/common/imgui_ext.h"

namespace aim {

bool PlaylistRunComponent(const std::string& id,
                          PlaylistRun* playlist_run,
                          std::string* scenario_to_start) {
  ImVec2 sz = ImVec2(0.0f, 0.0f);
  bool selected = false;
  for (int i = 0; i < playlist_run->playlist.def.items_size(); ++i) {
    PlaylistItemProgress& progress = playlist_run->progress_list[i];
    PlaylistItem item = playlist_run->playlist.def.items(i);
    std::string item_label = std::format("{}###{}_n{}", item.scenario(), id, i);
    if (ImGui::Button(item_label.c_str(), sz)) {
      selected = true;
      playlist_run->current_index = i;
      *scenario_to_start = item.scenario();
    }
    ImGui::SameLine();
    std::string progress_text = std::format("{}/{}", progress.runs_done, item.num_plays());
    ImGui::Text(progress_text);
  }
  return selected;
}

}  // namespace aim

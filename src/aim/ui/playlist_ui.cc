#include "playlist_ui.h"

#include <imgui.h>

#include "aim/common/imgui_ext.h"

namespace aim {
namespace {

class PlaylistEditorComponent : public UiComponent {
 public:
  explicit PlaylistEditorComponent(Application* app)
      : UiComponent(app), playlist_manager_(app->playlist_manager()) {}

  void Show() {}

 private:
  PlaylistManager* playlist_manager_;
};

class PlaylistComponentImpl : public UiComponent, public PlaylistComponent {
 public:
  explicit PlaylistComponentImpl(Application* app) : UiComponent(app) {}

  void Show(const std::string& playlist_name) override {}

 private:
  PlaylistManager* playlist_manager_;
};

}  // namespace

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

std::unique_ptr<PlaylistComponent> CreatePlaylistComponent(Application* app) {
  return std::make_unique<PlaylistComponentImpl>(app);
}

}  // namespace aim

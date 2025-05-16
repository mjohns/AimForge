#include "playlist_ui.h"

#include <imgui.h>

#include "aim/common/imgui_ext.h"

namespace aim {
namespace {

struct EditorResult {
  bool playlist_updated = false;
  bool editor_closed = false;
};

class PlaylistEditorComponent : public UiComponent {
 public:
  explicit PlaylistEditorComponent(Application* app, const std::string& playlist_name)
      : UiComponent(app),
        playlist_manager_(app->playlist_manager()),
        playlist_name_(playlist_name) {}

  void Show(EditorResult* result) {
    ImVec2 sz = ImVec2(0.0f, 0.0f);
    PlaylistRun* run = playlist_manager_->GetRun(playlist_name_);

    if (scenario_names_.size() == 0) {
      for (auto& i : run->playlist.def.items()) {
        scenario_names_.push_back(i.scenario());
      }
    }

    bool still_dragging = false;
    for (int i = 0; i < scenario_names_.size(); ++i) {
      const std::string& scenario_name = scenario_names_[i];
      std::string item_label =
          std::format("{}###Component{}PlaylistEditorItem{}", scenario_name, component_id_, i);
      ImGui::PushID(std::format("PlaylistItem{}", i).c_str());

      ImGui::Selectable(item_label.c_str());
      if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("PLAYLIST_ITEM_TYPE", &i, sizeof(int));
        ImGui::Text("Move \"%s\"", scenario_name.c_str());
        dragging_i_ = i;
        ImGui::EndDragDropSource();
      }
      if (ImGui::BeginDragDropTarget()) {
        ImGuiDragDropFlags drop_target_flags =
            ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("PLAYLIST_ITEM_TYPE", drop_target_flags)) {
          // Process the drop: 'i' is the index *after* which the item will be inserted
          int dragged_item_index = *(const int*)payload->Data;
          last_hovered_i_ = i;
          // Perform reordering or insertion logic here
          // Be careful with index validity and potential issues when modifying the list while
          // iterating

          ImVec2 rect_min = ImGui::GetItemRectMin();
          ImVec2 rect_max = ImGui::GetItemRectMax();

          float max_y = rect_max.y;
          float min_y = rect_min.y;
          float mouse_y = ImGui::GetMousePos().y;

          ImGuiIO& io = ImGui::GetIO();
          ImDrawList* draw_list = ImGui::GetWindowDrawList();
          float mid_y = min_y + (max_y - min_y) / 2.0;
          float draw_y = min_y;
          int dest_before_i = i;
          if (mouse_y > mid_y) {
            draw_y = max_y;
            dest_before_i++;
          }

          draw_list->AddLine(ImVec2(rect_min.x, draw_y),
                             ImVec2(rect_min.x + 200, draw_y),
                             ImGui::GetColorU32(ImGuiCol_DragDropTarget),
                             2.0f);

          if (payload->IsDelivery()) {
            scenario_names_ = MoveVectorItem(scenario_names_, dragging_i_, dest_before_i++);
            dragging_i_ = -1;
          }
        }

        ImGui::EndDragDropTarget();
      }
      bool still_dragging_item = dragging_i_ == i && ImGui::IsItemActive() &&
                                 ImGui::IsMouseDragging(ImGuiMouseButton_Left);
      if (still_dragging_item) {
        still_dragging = true;
      }

      ImGui::PopID();
    }

    if (!still_dragging) {
      dragging_i_ = -1;
    }
  }

 private:
  PlaylistManager* playlist_manager_;
  std::vector<std::string> scenario_names_;
  int dragging_i_ = -1;
  int last_hovered_i_ = -1;
  std::string playlist_name_;
};

class PlaylistComponentImpl : public UiComponent, public PlaylistComponent {
 public:
  explicit PlaylistComponentImpl(Application* app)
      : UiComponent(app), playlist_manager_(app->playlist_manager()) {}

  bool Show(const std::string& playlist_name, std::string* scenario_to_start) override {
    if (playlist_name != current_playlist_name_) {
      current_playlist_name_ = playlist_name;
      ResetForNewCurrentPlaylist();
    }

    if (showing_editor_) {
      if (!editor_component_) {
        editor_component_ = std::make_unique<PlaylistEditorComponent>(app_, playlist_name);
      }
      EditorResult editor_result;
      editor_component_->Show(&editor_result);
      if (editor_result.editor_closed) {
        editor_component_ = {};
        showing_editor_ = false;
        // Check if updated and handle appropriately.
      }
      return false;
    }

    ImGui::Text("%s", playlist_name.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Edit")) {
      showing_editor_ = true;
    }
    return PlaylistRunComponent(
        "PlaylistRun", playlist_manager_->GetCurrentRun(), scenario_to_start);
  }

 private:
  void ResetForNewCurrentPlaylist() {
    editor_component_ = {};
    showing_editor_ = false;
  }

  PlaylistManager* playlist_manager_;
  bool showing_editor_ = false;
  std::unique_ptr<PlaylistEditorComponent> editor_component_;
  std::string current_playlist_name_;
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

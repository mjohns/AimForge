#include "playlist_ui.h"

#include <imgui.h>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"

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
        playlist_name_(playlist_name) {
    PlaylistRun* run = playlist_manager_->GetRun(playlist_name_);
    if (run != nullptr) {
      new_playlist_name_ = run->playlist.bundle_playlist_name;
      for (auto& i : run->playlist.def.items()) {
        scenario_items_.push_back(i);
      }
    }
  }

  void Show(EditorResult* result) {
    PlaylistRun* run = playlist_manager_->GetRun(playlist_name_);
    if (run == nullptr) {
      result->editor_closed = true;
      return;
    }
    ImVec2 char_size = ImGui::CalcTextSize("A");

    ImGui::Text("%s", run->playlist.bundle_name.c_str());
    ImGui::SameLine();
    ImGui::PushItemWidth(400);
    ImGui::InputText("###PlaylistNameInput", &new_playlist_name_);
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Spacing();

    int remove_i = -1;
    bool still_dragging = false;
    for (int i = 0; i < scenario_items_.size(); ++i) {
      PlaylistItem& item = scenario_items_[i];
      const std::string& scenario_name = item.scenario();
      std::string item_label =
          std::format("{}###Component{}PlaylistEditorItem{}", scenario_name, component_id_, i);
      ImGui::PushID(std::format("PlaylistItem{}", i).c_str());

      if (i == dragging_i_) {
        ImGui::BeginDisabled();
        ImGui::Button(item_label.c_str());
        ImGui::EndDisabled();
      } else {
        ImGui::Button(item_label.c_str());
      }
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
            scenario_items_ = MoveVectorItem(scenario_items_, dragging_i_, dest_before_i);
            dragging_i_ = -1;
            updated_ = true;
          }
        }

        ImGui::EndDragDropTarget();
      }
      bool still_dragging_item = dragging_i_ == i && ImGui::IsItemActive() &&
                                 ImGui::IsMouseDragging(ImGuiMouseButton_Left);
      if (still_dragging_item) {
        still_dragging = true;
      }

      if (ImGui::BeginPopupContextItem("item_menu")) {
        if (ImGui::Selectable("Delete")) remove_i = i;
        ImGui::EndPopup();
      }
      ImGui::OpenPopupOnItemClick("item_menu", ImGuiPopupFlags_MouseButtonRight);

      ImGui::SameLine();
      u32 num_plays = item.num_plays();
      u32 step = 1;
      ImGui::SetNextItemWidth(char_size.x * 8);
      ImGui::InputScalar("###NumPlays", ImGuiDataType_U32, &num_plays, &step, nullptr, "%u");

      item.set_num_plays(num_plays);

      ImGui::PopID();
    }

    if (IsValidIndex(scenario_items_, remove_i)) {
      auto it = scenario_items_.begin() + remove_i;
      scenario_items_.erase(it);
    }

    if (!still_dragging) {
      dragging_i_ = -1;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Text("Add scenario");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_size.x * 30);
    ImGui::InputText("###AddScenarioInput", &scenario_search_text_);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
      scenario_search_text_ = "";
    }
    if (scenario_search_text_.size() > 0) {
      auto search_words = GetSearchWords(scenario_search_text_);
      ImGui::Indent();
      for (int i = 0; i < app_->scenario_manager()->scenarios().size(); ++i) {
        ImGui::PushID(std::format("ScenarioSearch{}", i).c_str());
        const auto& scenario = app_->scenario_manager()->scenarios()[i];
        if (StringMatchesSearch(scenario.name, search_words, /*empty_matches=*/false)) {
          if (ImGui::Button(scenario.name.c_str())) {
            PlaylistItem item;
            item.set_scenario(scenario.name);
            item.set_num_plays(1);
            scenario_items_.push_back(item);
            scenario_search_text_ = "";
          }
        }
        ImGui::PopID();
      }
      ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    if (ImGui::Button("Save")) {
      result->editor_closed = true;
      return;
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      result->editor_closed = true;
      return;
    }
  }

 private:
  PlaylistManager* playlist_manager_;
  std::vector<PlaylistItem> scenario_items_;
  int dragging_i_ = -1;
  std::string playlist_name_;
  bool updated_ = false;

  std::string scenario_search_text_;
  std::string new_playlist_name_;
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
  bool selected = false;
  for (int i = 0; i < playlist_run->playlist.def.items_size(); ++i) {
    PlaylistItemProgress& progress = playlist_run->progress_list[i];
    PlaylistItem item = playlist_run->playlist.def.items(i);
    std::string item_label = std::format("{}###{}_n{}", item.scenario(), id, i);
    if (ImGui::Button(item_label.c_str())) {
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

#include "playlist_editor_component.h"

#include <algorithm>

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/common/name_util.h"
#include "aim/ui/scenario_editor_screen.h"
#include "aim/ui/search_selector.h"
#include "google/protobuf/util/message_differencer.h"
#include "imgui.h"

namespace aim {
namespace {

class PlaylistEditorComponentImpl : public PlaylistEditorComponent {
 public:
  explicit PlaylistEditorComponentImpl(Screen& screen, const std::string& playlist_name)
      : app_(screen.app()), screen_(screen) {
    std::shared_ptr<PlaylistRun> run = app_.playlist_manager().GetRun(playlist_name);
    if (run != nullptr) {
      new_playlist_name_ = run->playlist.name.relative_name();
      original_playlist_name_ = run->playlist.name;
      bundle_name_ = run->playlist.name.bundle_name();
      auto maybe_playlist = app_.playlist_manager().GetPlaylist(run->playlist.name);
      if (maybe_playlist) {
        original_playlist_def_ = maybe_playlist->def();
        for (auto& i : maybe_playlist->items()) {
          scenario_items_.push_back(i);
        }
      }
    }
  }

  void Draw(EditorResult* result) {
    ImGui::IdGuard cid("PlaylistEditor");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Unable to save playlist");

      if (ImGui::Button("OK", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

    ImGui::AlignTextToFramePadding();
    ImGui::Text(bundle_name_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("###PlaylistNameInput", &new_playlist_name_);

    if (ImGui::Button("Save")) {
      if (SavePlaylist()) {
        result->editor_closed = true;
        result->playlist_updated = true;
        return;
      }
      ImGui::OpenPopup("Error");
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      result->editor_closed = true;
      return;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("PlaylistScrollableContent");
    DrawPlaylistScenariosEditor(result);
    ImGui::EndChild();
  }

 private:
  void DrawPlaylistScenariosEditor(EditorResult* result) {
    int remove_i = -1;
    bool still_dragging = false;
    for (int i = 0; i < scenario_items_.size(); ++i) {
      ImGui::IdGuard lid("PlaylistItem", i);
      PlaylistItem& item = scenario_items_[i];
      const std::string& scenario_name = item.scenario();

      if (i == dragging_i_) {
        ImGui::BeginDisabled();
        ImGui::Button(scenario_name);
        ImGui::EndDisabled();
      } else if (i == editing_i_) {
        if (focus_editor_) {
          // Fix issue where the first time it selects all the text when focusing
          // ImGui::SetKeyboardFocusHere();
          focus_editor_ = false;
        }
        bool enter_pressed = ImGui::InputText(
            "##ScenarioItemEditor", item.mutable_scenario(), ImGuiInputTextFlags_EnterReturnsTrue);
        if (enter_pressed) {
          editing_i_ = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button(kIconSave)) {
          editing_i_ = -1;
        }
      } else {
        if (ImGui::Button(scenario_name)) {
          editing_i_ = -1;
        }
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
          }
        }

        ImGui::EndDragDropTarget();
      }
      bool still_dragging_item = dragging_i_ == i && ImGui::IsItemActive() &&
                                 ImGui::IsMouseDragging(ImGuiMouseButton_Left);
      if (still_dragging_item) {
        still_dragging = true;
      }

      const char* item_menu = "PlaylistItemMenu";
      if (ImGui::BeginPopupContextItem(item_menu)) {
        if (ImGui::Selectable("Copy")) {
          scenario_items_.push_back(item);
        }
        if (ImGui::Selectable("Delete")) {
          remove_i = i;
        }
        if (ImGui::Selectable("Edit")) {
          editing_i_ = i;
          focus_editor_ = true;
        }
        if (ImGui::Selectable("Add levels")) {
          // std::optional<std::string> base_name = StripLevelSuffix
          //   item.scenario();
        }
        ImGui::EndPopup();
      }
      ImGui::OpenPopupOnItemClick(item_menu, ImGuiPopupFlags_MouseButtonRight);

      ImGui::SameLine();
      u32 num_plays = item.num_plays();
      u32 step = 1;
      ImGui::SetNextItemWidth(char_x_ * 8);
      ImGui::InputScalar("###NumPlays", ImGuiDataType_U32, &num_plays, &step, nullptr, "%u");

      item.set_num_plays(num_plays);
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
    ImGui::SetNextItemWidth(char_x_ * 18);
    ImGui::InputText("###AddScenarioInput", &scenario_search_text_);
    ImGui::SameLine();
    if (ImGui::Button(kIconCancel)) {
      scenario_search_text_ = "";
    }
    if (scenario_search_text_.size() > 0) {
      ImGui::Indent();
      auto scenario_names = app_.scenario_manager().scenario_names();
      ScenarioSelectorOptions options;
      options.additional_predicate = [&](const std::string& scenario_name) {
        bool already_in_playlist =
            std::any_of(scenario_items_.begin(), scenario_items_.end(), [=](const auto& item) {
              return item.scenario() == scenario_name;
            });
        return !already_in_playlist;
      };

      std::optional<std::string> selected_scenario =
          ScenarioSelector(scenario_search_text_, *scenario_names, options);
      if (selected_scenario) {
        PlaylistItem item;
        item.set_scenario(*selected_scenario);
        item.set_num_plays(1);
        scenario_items_.push_back(item);
      }
      ImGui::Unindent();
    }
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
  }

  bool SavePlaylist() {
    PlaylistDef playlist;

    playlist.mutable_items()->Add(scenario_items_.begin(), scenario_items_.end());

    ResourceName final_name(bundle_name_, new_playlist_name_);
    bool name_changed = final_name != original_playlist_name_;
    if (name_changed) {
      // Need to move file.
      std::vector<std::string> taken_names =
          app_.playlist_manager().GetAllRelativeNamesInBundle(bundle_name_);
      final_name.set(bundle_name_, MakeUniqueName(new_playlist_name_, taken_names));
      if (!app_.playlist_manager().RenamePlaylist(original_playlist_name_, final_name)) {
        return false;
      }
      app_.history_manager().UpdateRecentView(ObjectType::PLAYLIST, final_name.full_name());
      std::shared_ptr<PlaylistRun> current_run = app_.playlist_manager().GetCurrentRun();
      if (current_run != nullptr && current_run->playlist.name == original_playlist_name_) {
        app_.playlist_manager().SetCurrentPlaylist(final_name.full_name());
      }
    }

    return app_.playlist_manager().SavePlaylist(final_name, playlist);
  }

  Application& app_;
  Screen& screen_;
  std::vector<PlaylistItem> scenario_items_;
  int dragging_i_ = -1;
  int editing_i_ = -1;
  bool focus_editor_ = false;
  ResourceName original_playlist_name_;
  std::string bundle_name_;
  std::string source_base_scenario_;

  std::string scenario_search_text_;
  std::string new_playlist_name_;
  float char_x_ = 0;
  PlaylistDef original_playlist_def_;
};

}  // namespace

std::unique_ptr<PlaylistEditorComponent> CreatePlaylistEditorComponent(
    const std::string& playlist_name, UiScreen* screen) {
  return std::make_unique<PlaylistEditorComponentImpl>(*screen, playlist_name);
}

}  // namespace aim

#include "playlist_editor_component.h"

#include <algorithm>

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "aim/common/search.h"
#include "aim/core/playlist_manager.h"
#include "aim/ui/scenario_editor_screen.h"
#include "aim/ui/search_selector.h"
#include "imgui.h"

namespace aim {
namespace {

enum class PlaylistType {
  DEFAULT,
  LEVELS,
};

const std::vector<std::pair<PlaylistType, std::string>> kPlaylistTypes{
    {PlaylistType::DEFAULT, "Default"},
    {PlaylistType::LEVELS, "Levels"},
};

class PlaylistEditorComponentImpl : public PlaylistEditorComponent {
 public:
  explicit PlaylistEditorComponentImpl(Screen& screen, const std::string& playlist_name)
      : app_(screen.app()), screen_(screen) {
    std::shared_ptr<PlaylistRun> run = app_.playlist_manager().GetRun(playlist_name);
    if (run != nullptr) {
      ResourceName name = ResourceName::Parse(run->playlist.name);
      new_playlist_name_ = name.relative_name();
      original_playlist_name_ = run->playlist.name;
      bundle_name_ = name.bundle_name();
      auto maybe_playlist = app_.playlist_manager().GetPlaylist(run->playlist.name);
      if (maybe_playlist) {
        original_playlist_def_ = maybe_playlist->def();
        for (auto& i : maybe_playlist->items()) {
          scenario_items_.push_back(i);
        }
      }
      if (maybe_playlist->def().has_levels()) {
        levels_ = maybe_playlist->def().levels();
      }
    }
  }

  void Draw(EditorResult* result) {
    ImGui::IdGuard cid("PlaylistEditor");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    notification_popup_.Draw();

    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

    ImGui::AlignTextToFramePadding();
    ImGui::Text(bundle_name_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("###PlaylistNameInput", &new_playlist_name_);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Type");
    ImGui::SameLine();
    PlaylistType type = levels_.has_value() ? PlaylistType::LEVELS : PlaylistType::DEFAULT;
    if (ImGui::SimpleTypeDropdown("##TypeSelector", &type, kPlaylistTypes, char_x_ * 10)) {
      if (type == PlaylistType::LEVELS) {
        levels_ = LevelsPlaylistDef();
        auto& levels = *levels_;
        levels.set_max_level(10);
        levels.set_num_plays_per_level(1);
      }
      if (type == PlaylistType::DEFAULT) {
        levels_ = {};
      }
    }

    if (type == PlaylistType::LEVELS) {
      DrawLevelsEditor();
    }

    if (ImGui::Button("Save")) {
      if (SavePlaylist()) {
        result->editor_closed = true;
        result->playlist_updated = true;
        return;
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      result->editor_closed = true;
      return;
    }

    if (type == PlaylistType::DEFAULT) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
      ImGui::BeginChild("PlaylistScrollableContent");
      DrawPlaylistScenariosEditor(result);
      ImGui::EndChild();
    }
  }

 private:
  void DrawLevelsEditor() {
    if (!levels_) {
      levels_ = LevelsPlaylistDef();
    }

    auto& levels = *levels_;
    if (levels.max_level() < 1) {
      levels.set_max_level(1);
    }

    scenario_items_.clear();

    ImGui::IdGuard cid("LevelsEditor");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Base scenario");
    ImGui::SameLine();
    ImGui::InputText("###BaseScenarioInput", levels.mutable_base_scenario());
    if (levels.base_scenario().size() > 0) {
      ImGui::Indent();
      auto scenario_names = app_.scenario_manager().scenario_names();
      std::optional<std::string> selected_scenario =
          SearchSelector(levels.base_scenario(), *scenario_names);
      if (selected_scenario) {
        // set base_name
        levels.set_base_scenario(*selected_scenario);
      }
      ImGui::Unindent();
    }

    ImGui::Separator();

    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Max level")
                          .set_step(1, 2)
                          .set_min(2)
                          .set_default(10)
                          .set_precision(2)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(LevelsPlaylistDef, &levels, max_level));
    ImGui::InputInt(ImGui::InputIntParams::WithLabelAsId("Plays per level")
                        .set_step(1, 2)
                        .set_min(1)
                        .set_default(1)
                        .set_width(char_x_ * 10),
                    PROTO_INT_FIELD(LevelsPlaylistDef, &levels, num_plays_per_level));

    ImGui::Separator();
    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Min level")
                          .set_is_optional()
                          .set_step(1, 2)
                          .set_default(1)
                          .set_precision(2)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(LevelsPlaylistDef, &levels, min_level));
    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Step")
                          .set_step(0.5, 2)
                          .set_default(1)
                          .set_is_optional()
                          .set_precision(2)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(LevelsPlaylistDef, &levels, level_step));
  }

  void DrawPlaylistScenariosEditor(EditorResult* result) {
    int remove_i = -1;
    bool still_dragging = false;
    std::vector<PlaylistItem> items_to_add;
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
          items_to_add.push_back(item);
        }
        if (ImGui::Selectable("Delete")) {
          remove_i = i;
        }
        if (ImGui::Selectable("Edit")) {
          editing_i_ = i;
          focus_editor_ = true;
        }
        if (ImGui::Selectable("Add levels")) {
          NameInfo name_info = GetScenarioNameInfo(item.scenario());
          std::string cm_suffix =
              name_info.cm_per_360
                  ? std::format(" {}cm", MaybeIntToString(*name_info.cm_per_360, 1))
                  : "";
          float current_level = name_info.level ? *name_info.level : 0;
          for (int n = 0; n < 5; ++n) {
            current_level += 1.0f;
            PlaylistItem new_item;
            new_item.set_num_plays(item.num_plays());
            std::string new_name = AddLevelSuffix(name_info.base_name, current_level);
            new_item.set_scenario(std::format(
                "{} L{}{}", name_info.base_name, MaybeIntToString(current_level, 2), cm_suffix));
            items_to_add.push_back(new_item);
          }
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

    PushBackAll(&scenario_items_, items_to_add);

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
      SearchSelectorOptions options;
      options.additional_predicate = [&](const std::string& scenario_name) {
        bool already_in_playlist =
            std::any_of(scenario_items_.begin(), scenario_items_.end(), [=](const auto& item) {
              return item.scenario() == scenario_name;
            });
        return !already_in_playlist;
      };

      std::optional<std::string> selected_scenario =
          SearchSelector(scenario_search_text_, *scenario_names, options);
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

    if (levels_) {
      *playlist.mutable_levels() = *levels_;
    } else {
      playlist.mutable_items()->Add(scenario_items_.begin(), scenario_items_.end());
    }

    ResourceName final_name(bundle_name_, new_playlist_name_);
    NameInfo final_name_info = GetPlaylistNameInfo(final_name.full_name());
    if (final_name_info.HasDynamicSuffix()) {
      notification_popup_.NotifyOpen("Cannot name playlist with explicit cm/360 suffix.");
      return false;
    }

    bool name_changed = final_name.full_name() != original_playlist_name_;
    if (name_changed) {
      // Need to move file.
      std::vector<std::string> taken_names =
          app_.playlist_manager().GetAllRelativeNamesInBundle(bundle_name_);
      final_name.set(bundle_name_, MakeUniqueName(new_playlist_name_, taken_names));
      if (!app_.playlist_manager().RenamePlaylist(original_playlist_name_,
                                                  final_name.full_name())) {
        notification_popup_.NotifyOpen(
            std::format("Playlist with name \"{}\" already exists", final_name.full_name()));
        return false;
      }
      app_.history_manager().UpdateRecentView(ObjectType::PLAYLIST, final_name.full_name());
      std::shared_ptr<PlaylistRun> current_run = app_.playlist_manager().GetCurrentRun();
      if (current_run != nullptr && current_run->playlist.name == original_playlist_name_) {
        app_.playlist_manager().SetCurrentPlaylist(final_name.full_name());
      }
    }

    app_.playlist_manager().UpdatePlaylist(final_name.full_name(), playlist);
    bool saved = app_.bundle_manager().SaveDirtyBundles();
    if (!saved) {
      notification_popup_.NotifyOpen("Failed to save playlist to disk");
    }
    return saved;
  }

  Application& app_;
  Screen& screen_;
  std::vector<PlaylistItem> scenario_items_;
  int dragging_i_ = -1;
  int editing_i_ = -1;
  bool focus_editor_ = false;
  std::string original_playlist_name_;
  std::string bundle_name_;
  std::string source_base_scenario_;

  std::optional<LevelsPlaylistDef> levels_;
  std::string scenario_search_text_;
  std::string new_playlist_name_;
  float char_x_ = 0;
  PlaylistDef original_playlist_def_;
  ImGui::NotificationPopup notification_popup_{"Notification"};
};

}  // namespace

std::unique_ptr<PlaylistEditorComponent> CreatePlaylistEditorComponent(
    const std::string& playlist_name, UiScreen* screen) {
  return std::make_unique<PlaylistEditorComponentImpl>(*screen, playlist_name);
}

}  // namespace aim

#include "playlist_ui.h"

#include <imgui.h>

#include <algorithm>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/ui/scenario_editor.h"

namespace aim {
namespace {

const char* kDeleteConfirmationPopup = "DELETE_CONFIRMATION_POPUP";

std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name,
                                                     Application* app) {
  std::vector<std::string> names;
  for (const Playlist& playlist : app->playlist_manager()->playlists()) {
    if (playlist.name.bundle_name() == bundle_name) {
      names.push_back(playlist.name.relative_name());
    }
  }
  return names;
}

struct EditorResult {
  bool playlist_updated = false;
  bool editor_closed = false;
};

class PlaylistEditorComponent : public UiComponent {
 public:
  explicit PlaylistEditorComponent(Application* app, const std::string& playlist_name)
      : UiComponent(app), playlist_manager_(app->playlist_manager()) {
    PlaylistRun* run = playlist_manager_->GetRun(playlist_name);
    if (run != nullptr) {
      new_playlist_name_ = run->playlist.name.relative_name();
      original_playlist_name_ = run->playlist.name;
      bundle_name_ = run->playlist.name.bundle_name();
      for (auto& i : run->playlist.def.items()) {
        scenario_items_.push_back(i);
      }
    }
  }

  void Show(EditorResult* result) {
    auto cid = GetComponentIdGuard();

    ImVec2 char_size = ImGui::CalcTextSize("A");

    ImGui::AlignTextToFramePadding();
    ImGui::Text(bundle_name_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("###PlaylistNameInput", &new_playlist_name_);

    ImGui::Spacing();
    ImGui::Spacing();

    int remove_i = -1;
    bool still_dragging = false;
    for (int i = 0; i < scenario_items_.size(); ++i) {
      PlaylistItem& item = scenario_items_[i];
      const std::string& scenario_name = item.scenario();
      std::string item_label =
          std::format("{}###Component{}PlaylistEditorItem{}", scenario_name, component_id_, i);
      ImGui::IdGuard id("PlaylistItem", i);

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
        if (ImGui::Selectable("Delete")) remove_i = i;
        ImGui::EndPopup();
      }
      ImGui::OpenPopupOnItemClick(item_menu, ImGuiPopupFlags_MouseButtonRight);

      ImGui::SameLine();
      u32 num_plays = item.num_plays();
      u32 step = 1;
      ImGui::SetNextItemWidth(char_size.x * 8);
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
    ImGui::SetNextItemWidth(char_size.x * 30);
    ImGui::InputText("###AddScenarioInput", &scenario_search_text_);
    ImGui::SameLine();
    if (ImGui::Button(kIconCancel)) {
      scenario_search_text_ = "";
    }
    if (scenario_search_text_.size() > 0) {
      auto search_words = GetSearchWords(scenario_search_text_);
      ImGui::Indent();
      for (int i = 0; i < app_->scenario_manager()->scenarios().size(); ++i) {
        ImGui::IdGuard id("ScenarioSearch", i);
        const auto& scenario = app_->scenario_manager()->scenarios()[i];
        if (StringMatchesSearch(scenario.id(), search_words, /*empty_matches=*/false)) {
          bool already_in_playlist =
              std::any_of(scenario_items_.begin(), scenario_items_.end(), [=](const auto& item) {
                return item.scenario() == scenario.id();
              });
          if (!already_in_playlist) {
            if (ImGui::Button(scenario.id().c_str())) {
              PlaylistItem item;
              item.set_scenario(scenario.id());
              item.set_num_plays(1);
              scenario_items_.push_back(item);
            }
          }
        }
      }
      ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::Spacing();
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

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Unable to save playlist");

      if (ImGui::Button("OK", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }

 private:
  bool SavePlaylist() {
    PlaylistDef playlist;
    playlist.mutable_items()->Add(scenario_items_.begin(), scenario_items_.end());

    ResourceName final_name(bundle_name_, new_playlist_name_);
    bool name_changed = final_name != original_playlist_name_;
    if (name_changed) {
      // Need to move file.
      std::vector<std::string> taken_names = GetAllRelativeNamesInBundle(bundle_name_, app_);
      final_name.set(bundle_name_, MakeUniqueName(new_playlist_name_, taken_names));
      if (!playlist_manager_->RenamePlaylist(original_playlist_name_, final_name)) {
        return false;
      }
      PlaylistRun* current_run = playlist_manager_->GetCurrentRun();
      if (current_run != nullptr && current_run->playlist.name == original_playlist_name_) {
        playlist_manager_->SetCurrentPlaylist(final_name.full_name());
      }
    }

    return playlist_manager_->SavePlaylist(final_name, playlist);
  }

  PlaylistManager* playlist_manager_;
  std::vector<PlaylistItem> scenario_items_;
  int dragging_i_ = -1;
  ResourceName original_playlist_name_;
  std::string bundle_name_;

  std::string scenario_search_text_;
  std::string new_playlist_name_;
};

class PlaylistComponentImpl : public UiComponent, public PlaylistComponent {
 public:
  explicit PlaylistComponentImpl(UiScreen* screen)
      : UiComponent(screen->app()),
        playlist_manager_(screen->app()->playlist_manager()),
        screen_(screen) {}

  bool Show(const std::string& playlist_name, std::string* scenario_to_start) override {
    auto cid = GetComponentIdGuard();

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
        if (editor_result.playlist_updated) {
          playlist_manager_->LoadPlaylistsFromDisk();
        }
      }
      return false;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", playlist_name.c_str());
    ImGui::SameLine();
    if (ImGui::Button(kIconEdit)) {
      showing_editor_ = true;
    }
    ImGui::Spacing();
    ImGui::Spacing();
    PlaylistRunComponent2("PlaylistRun", playlist_manager_->GetCurrentRun(), screen_);
    return false;
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
  UiScreen* screen_;
};

class PlaylistListComponentImpl : public PlaylistListComponent {
 public:
  explicit PlaylistListComponentImpl(UiScreen* screen)
      : playlist_manager_(screen->app()->playlist_manager()),
        screen_(screen),
        app_(screen->app()) {}

  void Show(PlaylistListResult* result) override {
    delete_confirmation_dialog_.Draw("Delete", [=](const Playlist& playlist) {
      screen_->app()->playlist_manager()->DeletePlaylist(playlist.name);
      result->reload_playlists = true;
    });

    ImVec2 char_size = ImGui::CalcTextSize("A");
    ImGui::SetNextItemWidth(char_size.x * 30);
    ImGui::InputTextWithHint("##PlaylistSearchInput", kIconSearch, &playlist_search_text_);
    ImGui::SameLine();

    if (ImGui::Button(kIconRefresh)) {
      result->reload_playlists = true;
    }
    ImGui::HelpTooltip("Reload playlists from disk.");
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::BeginChild("PlaylistsContent");

    std::optional<Playlist> copy_playlist;
    auto search_words = GetSearchWords(playlist_search_text_);
    ImGui::LoopId loop_id;
    // TODO: group by bundle + collapse/expand all
    for (const auto& playlist : app_->playlist_manager()->playlists()) {
      auto id_guard = loop_id.Get();
      std::string name = playlist.name.full_name();
      if (StringMatchesSearch(name, search_words)) {
        bool is_selected = name == app_->playlist_manager()->current_playlist_name();
        if (ImGui::Selectable(name.c_str(), is_selected)) {
          result->open_playlist = playlist;
        }
        const char* menu_id = "PlaylistItemMenu";
        if (ImGui::BeginPopupContextItem(menu_id)) {
          if (ImGui::Selectable("Copy")) {
            copy_playlist = playlist;
          }
          if (ImGui::Selectable("Delete")) {
            delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", name), playlist);
          }
          ImGui::EndPopup();
        }
        ImGui::OpenPopupOnItemClick(menu_id, ImGuiPopupFlags_MouseButtonRight);
      }
    }

    ImGui::EndChild();

    if (copy_playlist.has_value()) {
      auto playlist = *copy_playlist;
      auto taken_names = GetAllRelativeNamesInBundle(playlist.name.bundle_name(), app_);
      std::string new_name = MakeUniqueName(playlist.name.relative_name() + " Copy", taken_names);
      app_->playlist_manager()->SavePlaylist(ResourceName(playlist.name.bundle_name(), new_name),
                                             playlist.def);
      app_->playlist_manager()->LoadPlaylistsFromDisk();
    }
  }

 private:
  ImGui::ConfirmationDialog<Playlist> delete_confirmation_dialog_{"DeleteConfirmationDialog"};
  std::string playlist_search_text_;
  PlaylistManager* playlist_manager_;
  UiScreen* screen_;
  Application* app_;
};

}  // namespace

bool PlaylistRunComponent(const std::string& id,
                          PlaylistRun* playlist_run,
                          std::string* scenario_to_start) {
  bool selected = false;
  for (int i = 0; i < playlist_run->playlist.def.items_size(); ++i) {
    ImGui::IdGuard id(i);
    PlaylistItemProgress& progress = playlist_run->progress_list[i];
    PlaylistItem item = playlist_run->playlist.def.items(i);
    float width = std::min(ImGui::GetContentRegionAvail().x, 600.0f);
    float right = ImGui::GetCursorPosX() + width;
    ImGui::SetNextItemWidth(width);
    if (ImGui::Selectable(item.scenario().c_str(), i == playlist_run->current_index)) {
      selected = true;
      playlist_run->current_index = i;
      *scenario_to_start = item.scenario();
    }

    ImGui::SameLine();
    std::string progress_text = std::format("{}/{}", progress.runs_done, item.num_plays());

    float len = ImGui::CalcTextSize(progress_text.c_str()).x;
    ImGui::SetCursorPosX(right - len);
    ImGui::Text(progress_text);
  }
  return selected;
}

void PlaylistRunComponent2(const std::string& id, PlaylistRun* playlist_run, Screen* screen) {
  ImGui::IdGuard cid(id);
  for (int i = 0; i < playlist_run->playlist.def.items_size(); ++i) {
    ImGui::IdGuard id(i);
    PlaylistItemProgress& progress = playlist_run->progress_list[i];
    PlaylistItem item = playlist_run->playlist.def.items(i);
    float width = std::min(ImGui::GetContentRegionAvail().x, 600.0f);
    float right = ImGui::GetCursorPosX() + width;
    ImGui::SetNextItemWidth(width);
    if (ImGui::Selectable(item.scenario().c_str(), i == playlist_run->current_index)) {
      playlist_run->current_index = i;
      screen->state()->scenario_run_option = ScenarioRunOption::START_CURRENT;
      screen->app()->scenario_manager()->SetCurrentScenario(item.scenario());
      screen->ReturnHome();
    }

    const char* popup_id = "ScenarioItemMenu";
    if (ImGui::BeginPopupContextItem(popup_id)) {
      if (ImGui::Selectable("Edit")) {
        ScenarioEditorOptions opts;
        opts.scenario_id = item.scenario();
        screen->PushNextScreen(CreateScenarioEditorScreen(opts, screen->app()));
      }
      if (ImGui::Selectable("Edit new copy")) {
        ScenarioEditorOptions opts;
        opts.scenario_id = item.scenario();
        opts.is_new_copy = true;
        screen->PushNextScreen(CreateScenarioEditorScreen(opts, screen->app()));
      }
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);

    ImGui::SameLine();
    std::string progress_text = std::format("{}/{}", progress.runs_done, item.num_plays());

    float len = ImGui::CalcTextSize(progress_text.c_str()).x;
    ImGui::SetCursorPosX(right - len);
    ImGui::Text(progress_text);
  }
}

std::unique_ptr<PlaylistComponent> CreatePlaylistComponent(UiScreen* screen) {
  return std::make_unique<PlaylistComponentImpl>(screen);
}

std::unique_ptr<PlaylistListComponent> CreatePlaylistListComponent(UiScreen* screen) {
  return std::make_unique<PlaylistListComponentImpl>(screen);
}

}  // namespace aim

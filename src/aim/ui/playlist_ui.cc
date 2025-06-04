#include "playlist_ui.h"

#include <imgui.h>

#include <algorithm>

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/ui/scenario_editor_screen.h"

namespace aim {
namespace {

const char* kDeleteConfirmationPopup = "DELETE_CONFIRMATION_POPUP";

std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name,
                                                     Application* app) {
  std::vector<std::string> names;
  for (const Playlist& playlist : app->playlist_manager().playlists()) {
    if (playlist.name.bundle_name() == bundle_name) {
      names.push_back(playlist.name.relative_name());
    }
  }
  return names;
}

struct CopyPlaylistOptions {
  std::string remove_prefix;
  std::string add_prefix;
  bool deep_copy;
};

bool CopyPlaylist(Playlist source,
                  ResourceName new_playlist_name,
                  const CopyPlaylistOptions& opts,
                  Application& app) {
  // Copy all scenarios if necessary.
  auto taken_names = GetAllRelativeNamesInBundle(new_playlist_name.bundle_name(), &app);
  *new_playlist_name.mutable_relative_name() =
      MakeUniqueName(new_playlist_name.relative_name(), taken_names);

  PlaylistDef dest = source.def;
  if (opts.deep_copy) {
    dest.clear_items();
    for (const auto& source_item : source.def.items()) {
      auto source_scenario = app.scenario_manager().GetScenario(source_item.scenario());
      if (!source_scenario) {
        // Skip invalid scenarios.
        continue;
      }
      ResourceName new_scenario_name = source_scenario->name;
      *new_scenario_name.mutable_bundle_name() = new_playlist_name.bundle_name();

      std::string* relative_name = new_scenario_name.mutable_relative_name();
      if (opts.remove_prefix.size() > 0) {
        *relative_name = absl::StripLeadingAsciiWhitespace(
            absl::StripPrefix(*relative_name, opts.remove_prefix));
      }
      if (opts.add_prefix.size() > 0) {
        *relative_name = std::format("{} {}", opts.add_prefix, *relative_name);
      }

      auto maybe_final_scenario_name = app.scenario_manager().SaveScenarioWithUniqueName(
          new_scenario_name, source_scenario->def);
      if (maybe_final_scenario_name) {
        PlaylistItem item = source_item;
        item.set_scenario(maybe_final_scenario_name->full_name());
        *dest.add_items() = item;
      }
    }
  }
  app.playlist_manager().SavePlaylist(new_playlist_name, dest);
  return true;
}

struct EditorResult {
  bool playlist_updated = false;
  bool editor_closed = false;
};

class PlaylistEditorComponent {
 public:
  explicit PlaylistEditorComponent(Application& app, const std::string& playlist_name) : app_(app) {
    PlaylistRun* run = app.playlist_manager().GetRun(playlist_name);
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
    ImGui::IdGuard cid("PlaylistEditor");

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
      ImGui::IdGuard lid("PlaylistItem", i);
      PlaylistItem& item = scenario_items_[i];
      const std::string& scenario_name = item.scenario();

      if (i == dragging_i_) {
        ImGui::BeginDisabled();
        ImGui::Button(scenario_name);
        ImGui::EndDisabled();
      } else {
        ImGui::Button(scenario_name);
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
      for (int i = 0; i < app_.scenario_manager().scenarios().size(); ++i) {
        ImGui::IdGuard id("ScenarioSearch", i);
        const auto& scenario = app_.scenario_manager().scenarios()[i];
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
      std::vector<std::string> taken_names = GetAllRelativeNamesInBundle(bundle_name_, &app_);
      final_name.set(bundle_name_, MakeUniqueName(new_playlist_name_, taken_names));
      if (!app_.playlist_manager().RenamePlaylist(original_playlist_name_, final_name)) {
        return false;
      }
      PlaylistRun* current_run = app_.playlist_manager().GetCurrentRun();
      if (current_run != nullptr && current_run->playlist.name == original_playlist_name_) {
        app_.playlist_manager().SetCurrentPlaylist(final_name.full_name());
      }
    }

    return app_.playlist_manager().SavePlaylist(final_name, playlist);
  }

  Application& app_;
  std::vector<PlaylistItem> scenario_items_;
  int dragging_i_ = -1;
  ResourceName original_playlist_name_;
  std::string bundle_name_;

  std::string scenario_search_text_;
  std::string new_playlist_name_;
};

class PlaylistComponentImpl : public PlaylistComponent {
 public:
  explicit PlaylistComponentImpl(UiScreen& screen) : app_(screen.app()), screen_(screen) {}

  bool Show(const std::string& playlist_name, std::string* scenario_to_start) override {
    ImGui::IdGuard cid("PlaylistComponent");

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
          app_.playlist_manager().LoadPlaylistsFromDisk();
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
    PlaylistRunComponent("PlaylistRun", app_.playlist_manager().GetCurrentRun(), screen_);
    return false;
  }

 private:
  void ResetForNewCurrentPlaylist() {
    editor_component_ = {};
    showing_editor_ = false;
  }

  bool showing_editor_ = false;
  std::unique_ptr<PlaylistEditorComponent> editor_component_;
  std::string current_playlist_name_;
  UiScreen& screen_;
  Application& app_;
};

class PlaylistListComponentImpl : public PlaylistListComponent {
 public:
  explicit PlaylistListComponentImpl(UiScreen& screen) : screen_(screen), app_(screen.app()) {}

  void Show(PlaylistListResult* result) override {
    delete_confirmation_dialog_.Draw("Delete", [=](const Playlist& playlist) {
      screen_.app().playlist_manager().DeletePlaylist(playlist.name);
      result->reload_playlists = true;
    });

    if (copy_dialog_.Draw(app_)) {
      result->reload_playlists = true;
    }

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

    auto search_words = GetSearchWords(playlist_search_text_);
    ImGui::LoopId loop_id;
    // TODO: group by bundle + collapse/expand all
    for (const auto& playlist : app_.playlist_manager().playlists()) {
      auto id_guard = loop_id.Get();
      std::string name = playlist.name.full_name();
      if (StringMatchesSearch(name, search_words)) {
        bool is_selected = name == app_.playlist_manager().current_playlist_name();
        if (ImGui::Selectable(name.c_str(), is_selected)) {
          result->open_playlist = playlist;
        }
        const char* menu_id = "PlaylistItemMenu";
        if (ImGui::BeginPopupContextItem(menu_id)) {
          if (ImGui::Selectable("Copy")) {
            copy_dialog_.NotifyOpen(playlist);
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
  }

 private:
  ImGui::ConfirmationDialog<Playlist> delete_confirmation_dialog_{"DeleteConfirmationDialog"};
  std::string playlist_search_text_;
  UiScreen& screen_;
  Application& app_;
  CopyPlaylistDialog copy_dialog_{"CopyPlaylistDialog"};
};

}  // namespace

void PlaylistRunRightClickMenu(const std::string& scenario_id, PlaylistRun* run, Screen& screen) {
  const char* popup_id = "ScenarioItemMenu";
  if (ImGui::BeginPopupContextItem(popup_id)) {
    if (ImGui::Selectable("Edit")) {
      ScenarioEditorOptions opts;
      opts.scenario_id = scenario_id;
      screen.PushNextScreen(CreateScenarioEditorScreen(opts, &screen.app()));
    }
    if (ImGui::Selectable("Edit new copy")) {
      ScenarioEditorOptions opts;
      opts.scenario_id = scenario_id;
      opts.is_new_copy = true;
      screen.PushNextScreen(CreateScenarioEditorScreen(opts, &screen.app()));
    }
    if (ImGui::BeginMenu("Add to")) {
      std::string selected_playlist;
      int playlist_count = 0;
      const auto& recent_playlists = screen.app().history_manager().recent_playlists();
      for (int i = 0; i < std::min<int>(6, recent_playlists.size()); ++i) {
        const std::string& playlist_name = recent_playlists[i];
        ImGui::IdGuard playlist_id(playlist_name, i);
        if (run->playlist.name.full_name() != playlist_name &&
            ImGui::MenuItem(playlist_name.c_str())) {
          selected_playlist = playlist_name;
        }
      }
      if (selected_playlist.size() > 0) {
        screen.app().playlist_manager().AddScenarioToPlaylist(selected_playlist, scenario_id);
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
}

void PlaylistRunComponent(const std::string& id, PlaylistRun* run, Screen& screen) {
  ImGui::IdGuard cid(id);
  const PlaylistDef& playlist = run->playlist.def;
  ImGuiTableFlags flags = ImGuiTableFlags_RowBg;
  std::string sample_progress_text = "00/00";
  float progress_width = ImGui::CalcTextSize(sample_progress_text.c_str()).x;

  if (!ImGui::BeginTable("PlaylistRuns", 2, flags)) {
    return;
  }
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, progress_width);

  std::vector<float> score_levels;
  bool has_score_level = false;
  for (const auto& item : playlist.items()) {
    auto stats = screen.app().stats_manager().GetAggregateStats(item.scenario());
    float level = 0;
    if (stats.total_runs > 0) {
      auto scenario = screen.app().scenario_manager().GetScenario(item.scenario());
      if (scenario) {
        level = GetScenarioScoreLevel(stats.high_score_stats.score, scenario->def);
        if (level > 0) {
          has_score_level = true;
        }
      }
    }
    score_levels.push_back(level);
  }

  for (int i = 0; i < playlist.items_size(); ++i) {
    ImGui::TableNextRow();
    ImGui::IdGuard id(i);
    PlaylistItemProgress& progress = run->progress_list[i];
    PlaylistItem item = playlist.items(i);
    bool is_selected = i == run->current_index;

    ImGui::TableNextColumn();
    std::string label = item.scenario();
    if (score_levels[i] > 0) {
      label = std::format("{} -- {}", label, MaybeIntToString(score_levels[i], 1));
    }
    if (ImGui::Selectable(label.c_str(), is_selected)) {
      run->current_index = i;
      screen.state().scenario_run_option = ScenarioRunOption::START_CURRENT;
      screen.app().scenario_manager().SetCurrentScenario(item.scenario());
      screen.ReturnHome();
    }
    PlaylistRunRightClickMenu(item.scenario(), run, screen);

    ImGui::TableNextColumn();
    std::string progress_text = std::format("{}/{}", progress.runs_done, item.num_plays());
    ImGui::Text(progress_text);
  }

  ImGui::EndTable();
}

std::unique_ptr<PlaylistComponent> CreatePlaylistComponent(UiScreen* screen) {
  return std::make_unique<PlaylistComponentImpl>(*screen);
}

std::unique_ptr<PlaylistListComponent> CreatePlaylistListComponent(UiScreen* screen) {
  return std::make_unique<PlaylistListComponentImpl>(*screen);
}

bool CopyPlaylistDialog::Draw(Application& app) {
  ImGui::IdGuard cid("CopyPlaylistDialogContent");
  bool did_copy = false;
  bool show_popup = source_.has_value();
  if (show_popup) {
    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(id_.c_str(),
                               &show_popup,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
      ImGui::TextFmt("Copy \"{}\" to", source_->name.full_name());
      ImGui::SimpleDropdown("BundlePicker",
                            new_name_.mutable_bundle_name(),
                            bundle_names_,
                            ImGui::GetFrameHeight() * 9);
      ImGui::SameLine();
      ImGui::InputText("##RelativeNameInput", new_name_.mutable_relative_name());

      ImGui::Indent();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Make new copies of all scenarios");
      ImGui::SameLine();
      ImGui::Checkbox("##DeepCopy", &deep_copy_);

      if (deep_copy_) {
        ImGui::Indent();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Add name prefix*");
        ImGui::SameLine();
        ImGui::InputText("##AddPrefix", &add_prefix_);
        ImGui::SameLine();
        ImGui::HelpMarker(
            "Adds the following prefix to all newly created scenario names after the bundle name");

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Remove name prefix");
        ImGui::SameLine();
        ImGui::InputText("##RemovePrefix", &remove_prefix_);
        ImGui::SameLine();
        ImGui::HelpMarker("Strips the following prefix from the name of scenarios being copied");

        ImGui::Unindent();
      }

      ImGui::Unindent();
      bool cant_copy = deep_copy_ && add_prefix_.size() == 0;
      if (cant_copy) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button("Copy")) {
        // Do copy
        CopyPlaylistOptions opts;
        opts.add_prefix = add_prefix_;
        opts.remove_prefix = remove_prefix_;
        opts.deep_copy = deep_copy_;
        CopyPlaylist(*source_, new_name_, opts, app);
        did_copy = true;
        ImGui::CloseCurrentPopup();
        source_ = {};
      }
      if (cant_copy) {
        ImGui::EndDisabled();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        source_ = {};
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
  if (open_) {
    ImGui::OpenPopup(id_.c_str());
    open_ = false;
    deep_copy_ = false;
    remove_prefix_;
    add_prefix_;
    bundle_names_ = app.file_system()->GetBundleNames();
    new_name_ = source_->name;
    *new_name_.mutable_relative_name() += " Copy";
  }
  return did_copy;
}

}  // namespace aim

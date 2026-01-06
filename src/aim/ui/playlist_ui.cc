#include "playlist_ui.h"

#include <algorithm>

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "aim/common/search.h"
#include "aim/core/stats_manager.h"
#include "aim/ui/copy_playlist_dialog.h"
#include "aim/ui/playlist_editor_component.h"
#include "aim/ui/scenario_editor_screen.h"
#include "aim/ui/select_sensitivity_variation_dialog.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace aim {
namespace {

const char* kPlaylistViewTypeKey = "PlaylistViewType";

enum class PlaylistViewType : int {
  RECENT = 1,
  ALL = 2,
  STARRED = 3,
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
        editor_component_ = CreatePlaylistEditorComponent(playlist_name, &screen_);
      }
      EditorResult editor_result;
      editor_component_->Draw(&editor_result);
      if (editor_result.editor_closed) {
        editor_component_ = {};
        showing_editor_ = false;
      }
      return false;
    }

    std::shared_ptr<PlaylistRun> run = app_.playlist_manager().GetCurrentRun();

    std::optional<float> selected_sensitivity;
    if (select_sensitivity_variation_dialog_.Draw(&selected_sensitivity)) {
      NameInfo name_info = GetPlaylistNameInfo(current_playlist_name_);
      name_info.cm_per_360 = selected_sensitivity;
      std::string new_name = name_info.GetFullName();
      if (new_name != current_playlist_name_) {
        app_.playlist_manager().SetCurrentPlaylist(new_name);
        app_.history_manager().UpdateRecentView(ObjectType::PLAYLIST, new_name);
      }
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text(current_playlist_name_);

    if (!run->playlist.cm_per_360) {
      ImGui::SameLine();
      if (ImGui::Button(kIconEdit)) {
        showing_editor_ = true;
      }
      ImGui::HelpTooltip("Edit playlist");
    }

    ImGui::SameLine();
    if (ImGui::Button(kIconMouse)) {
      NameInfo name_info = GetPlaylistNameInfo(current_playlist_name_);
      select_sensitivity_variation_dialog_.NotifyOpen(name_info.cm_per_360);
    }
    ImGui::HelpTooltip("Set cm/360 playlist variation");

    ImGui::SameLine();
    if (ImGui::Button(kIconRedo)) {
      app_.playlist_manager().ClearRun(run->playlist.name);
      run = app_.playlist_manager().GetCurrentRun();
    }
    ImGui::HelpTooltip("Reset current run");

    ImGui::SameLine();
    if (ImGui::Button(kIconShuffle)) {
      run->Shuffle(app_.rand());
    }
    ImGui::HelpTooltip("Shuffle playlist order");

    ImGui::Spacing();
    ImGui::Spacing();
    PlaylistRunComponent("PlaylistRun", run, screen_);
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
  NameInfo current_playlist_name_info_;
  SelectSensitivityVariationDialog select_sensitivity_variation_dialog_{
      "SelectPlaylistSensitivityDialog"};
  UiScreen& screen_;
  Application& app_;
};

class PlaylistListComponentImpl : public PlaylistListComponent {
 public:
  explicit PlaylistListComponentImpl(UiScreen& screen) : screen_(screen), app_(screen.app()) {
    auto maybe_initial_view_type = app_.local_store().GetInt(kPlaylistViewTypeKey);
    if (maybe_initial_view_type) {
      view_type_ = static_cast<PlaylistViewType>(*maybe_initial_view_type);
    } else {
      view_type_ = PlaylistViewType::ALL;
    }
  }

  void Show(PlaylistListResult* result) override {
    delete_confirmation_dialog_.Draw("Delete", [=](const Playlist& playlist) {
      screen_.app().playlist_manager().DeletePlaylist(playlist.name);
      // result->reload_playlists = true;
    });

    if (copy_dialog_.Draw(app_)) {
      // result->reload_playlists = true;
    }
    if (add_dialog_.Draw(app_)) {
      // result->reload_playlists = true;
    }

    ImVec2 char_size = ImGui::CalcTextSize("A");

    ImGui::Spacing();
    if (ImGui::Button(std::format("{} Add playlist", kIconAdd))) {
      add_dialog_.NotifyOpen();
    }

    if (app_.history_manager().recent_playlists().size() > 0) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      // Draw the 10 most recent items.
      ImGui::LoopId loop_id;
      int i = 0;
      for (const std::string& name : app_.history_manager().recent_playlists()) {
        i++;
        if (i >= 10) {
          break;
        }
        auto id_guard = loop_id.Get();
        DrawPlaylistItem(name, result);
      }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(char_size.x * 8);
    if (ImGui::SimpleTypeDropdown("##PlaylistViewType",
                                  &view_type_,
                                  {
                                      {PlaylistViewType::RECENT, "Recent"},
                                      {PlaylistViewType::STARRED, "Starred"},
                                      {PlaylistViewType::ALL, "All"},
                                  })) {
      app_.local_store().PutInt(kPlaylistViewTypeKey, (int)view_type_);
    }

    ImGui::Spacing();

    ImGui::SetNextItemWidth(char_size.x * 30);
    ImGui::InputTextWithHint("##PlaylistSearchInput", kIconSearch, &playlist_search_text_);
    if (playlist_search_text_.size() > 0) {
      ImGui::SameLine();
      if (ImGui::Button(kIconCancel)) {
        playlist_search_text_ = "";
      }
    }

    ImGui::Spacing();

    ImGui::BeginChild("PlaylistsContent");

    auto search_words = GetSearchWords(playlist_search_text_);
    ImGui::LoopId loop_id;

    if (view_type_ == PlaylistViewType::RECENT) {
      for (const std::string& name : app_.history_manager().recent_playlists()) {
        auto id_guard = loop_id.Get();
        if (StringMatchesSearch(name, search_words)) {
          DrawPlaylistItem(name, result);
        }
      }
    } else if (view_type_ == PlaylistViewType::STARRED) {
      auto items = app_.labels_manager().ListStarredItems(ObjectType::PLAYLIST);
      for (const std::string& name : items->items) {
        auto id_guard = loop_id.Get();
        if (StringMatchesSearch(name, search_words)) {
          DrawPlaylistItem(name, result);
        }
      }
    } else {
      for (const std::string& name : *app_.playlist_manager().playlist_names()) {
        auto id_guard = loop_id.Get();
        if (StringMatchesSearch(name, search_words)) {
          DrawPlaylistItem(name, result);
        }
      }
    }

    ImGui::EndChild();
  }

  void DrawPlaylistItem(const std::string& playlist_name, PlaylistListResult* result) {
    auto playlist = app_.playlist_manager().GetPlaylist(playlist_name);
    if (!playlist) {
      return;
    }
    if (ImGui::Button(playlist_name.c_str())) {
      result->open_playlist = *playlist;
    }
    const char* menu_id = "PlaylistItemMenu";
    if (ImGui::BeginPopupContextItem(menu_id)) {
      if (ImGui::Selectable("Copy")) {
        auto playlist = app_.playlist_manager().GetPlaylist(playlist_name);
        if (playlist) {
          copy_dialog_.NotifyOpen(*playlist);
        }
      }
      if (ImGui::Selectable("Delete")) {
        auto playlist = app_.playlist_manager().GetPlaylist(playlist_name);
        if (playlist) {
          delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", playlist_name),
                                                 *playlist);
        }
      }
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick(menu_id, ImGuiPopupFlags_MouseButtonRight);

    ImGui::SameLine();
    if (app_.labels_manager().IsStarred(ObjectType::PLAYLIST, playlist_name)) {
      if (ImGui::Selectable(kIconStar, false, 0, ImVec2(ImGui::GetTextLineHeight(), 0))) {
        app_.labels_manager().UnstarItem(ObjectType::PLAYLIST, playlist_name);
      }
    } else {
      if (ImGui::Selectable(kIconStarOutline, false, 0, ImVec2(ImGui::GetTextLineHeight(), 0))) {
        app_.labels_manager().StarItem(ObjectType::PLAYLIST, playlist_name);
      }
    }
  }

 private:
  ImGui::ConfirmationDialog<Playlist> delete_confirmation_dialog_{"DeleteConfirmationDialog"};
  std::string playlist_search_text_;
  UiScreen& screen_;
  Application& app_;
  CopyPlaylistDialog copy_dialog_{"CopyPlaylistDialog"};
  AddPlaylistDialog add_dialog_{"AddPlaylistDialog"};
  PlaylistViewType view_type_ = PlaylistViewType::ALL;
};

}  // namespace

void PlaylistRunRightClickMenu(const std::string& scenario_id, PlaylistRun& run, Screen& screen) {
  const char* popup_id = "ScenarioItemMenu";
  bool is_levels_playlist = run.playlist.def().has_levels();
  if (ImGui::BeginPopupContextItem(popup_id)) {
    if (ImGui::Selectable("Edit")) {
      ScenarioEditorOptions opts;
      opts.scenario_name = scenario_id;
      screen.PushNextScreen(CreateScenarioEditorScreen(opts, &screen.app()));
    }
    if (ImGui::Selectable("Edit new copy")) {
      ScenarioEditorOptions opts;
      opts.scenario_name = scenario_id;
      opts.is_new_copy = true;
      screen.PushNextScreen(CreateScenarioEditorScreen(opts, &screen.app()));
    }
    if (!is_levels_playlist) {
      if (ImGui::Selectable("Add new copy")) {
        ScenarioEditorOptions opts;
        opts.scenario_name = scenario_id;
        opts.is_new_copy = true;
        opts.add_to_playlist = run.playlist.name;
        opts.force_bundle_name = ResourceName::Parse(opts.add_to_playlist).bundle_name();
        screen.PushNextScreen(CreateScenarioEditorScreen(opts, &screen.app()));
      }
    }
    if (ImGui::BeginMenu("Add to")) {
      std::string selected_playlist;
      int playlist_count = 0;
      const auto& recent_playlists = screen.app().history_manager().recent_playlists();
      for (int i = 0; i < recent_playlists.size(); ++i) {
        const std::string& playlist_name = recent_playlists[i];
        auto maybe_playlist = screen.app().playlist_manager().GetPlaylist(playlist_name);
        if (!maybe_playlist.has_value() || maybe_playlist->def().has_levels()) {
          continue;
        }
        if (playlist_count >= 6) {
          break;
        }
        ImGui::IdGuard playlist_id(playlist_name, i);
        playlist_count++;
        if (run.playlist.name != playlist_name && ImGui::MenuItem(playlist_name.c_str())) {
          selected_playlist = playlist_name;
        }
      }
      if (selected_playlist.size() > 0) {
        screen.app().playlist_manager().AddScenarioToPlaylist(selected_playlist, scenario_id);
        screen.app().bundle_manager().SaveBundle(
            ResourceName::Parse(selected_playlist).bundle_name());
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
}

void PlaylistRunComponent(const std::string& id, std::shared_ptr<PlaylistRun> run, Screen& screen) {
  ImGui::IdGuard cid(id);
  std::string sample_progress_text = "00/00";
  float progress_width = ImGui::CalcTextSize(sample_progress_text.c_str()).x;

  auto& progress_items = run->progress_list;

  std::vector<float> score_levels;
  bool has_score_level = false;
  for (const auto& item : progress_items) {
    auto stats = screen.app().stats_manager().GetAggregateStats(item.item.scenario());
    float level = 0;
    if (stats.total_runs > 0) {
      auto scenario_def =
          screen.app().scenario_manager().GetEvaluatedScenarioDef(item.item.scenario());
      if (scenario_def) {
        level = GetScenarioScoreLevel(stats.high_score_stats.score, *scenario_def);
        if (level > 0) {
          has_score_level = true;
        }
      }
    }
    score_levels.push_back(level);
  }

  ImGuiTableFlags flags = ImGuiTableFlags_RowBg;
  if (!ImGui::BeginTable("PlaylistRuns", has_score_level ? 3 : 2, flags)) {
    return;
  }
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, progress_width);
  if (has_score_level) {
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, progress_width);
  }

  for (int i = 0; i < progress_items.size(); ++i) {
    ImGui::TableNextRow();
    ImGui::IdGuard id(i);
    PlaylistItemProgress& progress = run->progress_list[i];
    PlaylistItem& item = progress_items[i].item;
    bool is_selected = i == run->current_index;

    ImGui::TableNextColumn();
    std::string label = item.scenario();
    if (ImGui::Selectable(label.c_str(), is_selected)) {
      run->current_index = i;
      screen.state().scenario_run_option = ScenarioRunOption::START_CURRENT;
      screen.app().scenario_manager().SetCurrentScenario(item.scenario());
      screen.ReturnHome();
    }
    PlaylistRunRightClickMenu(item.scenario(), *run, screen);

    if (has_score_level) {
      ImGui::TableNextColumn();
      if (score_levels[i] > 0) {
        ImGui::TextAligned(
            1.0f,
            -FLT_MIN,
            "%s",
            std::format("{}{}", MaybeIntToString(score_levels[i], 1), kIconBolt).c_str());
      }
    }

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

bool AddPlaylistDialog::Draw(Application& app) {
  ImGui::IdGuard cid("AddPlaylistDialogContent");
  bool did_add = false;
  if (is_open_) {
    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(id_.c_str(),
                               &is_open_,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
      ImGui::SimpleDropdown(
          "BundlePicker", name_.mutable_bundle_name(), bundle_names_, ImGui::GetFrameHeight() * 9);
      ImGui::SameLine();
      ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());

      if (ImGui::Button("Add")) {
        auto taken_names = app.playlist_manager().GetAllRelativeNamesInBundle(name_.bundle_name());
        *name_.mutable_relative_name() = MakeUniqueName(name_.relative_name(), taken_names);
        app.playlist_manager().UpdatePlaylist(name_.full_name(), PlaylistDef());
        app.playlist_manager().SetCurrentPlaylist(name_.full_name());
        app.history_manager().UpdateRecentView(ObjectType::PLAYLIST, name_.full_name());
        did_add = true;
        ImGui::CloseCurrentPopup();
        is_open_ = false;
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        is_open_ = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
  if (open_) {
    ImGui::OpenPopup(id_.c_str());
    open_ = false;
    is_open_ = true;
    bundle_names_ = app.file_system()->GetBundleNames();
    name_.set("USER", "New playlist");
  }
  return did_add;
}

}  // namespace aim

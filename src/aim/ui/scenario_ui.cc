#include "scenario_ui.h"

#include "absl/algorithm/container.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/name_util.h"
#include "aim/common/search.h"
#include "aim/common/times.h"
#include "aim/core/application.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/stats_manager.h"
#include "aim/ui/editor/scenario_editor_common.h"
#include "aim/ui/editor/scenario_editor_screen.h"
#include "aim/ui/select_variation_dialog.h"
#include "aim/ui/stats/stats_screen.h"
#include "imgui.h"

namespace aim {
namespace {

enum class ScenarioViewType : int {
  RECENT = 1,
  STARRED = 2,
  ALL = 3,
};

class CreateLevelsPlaylistDialog {
 public:
  void NotifyOpen(const std::string& scenario_name) {
    open_ = true;
    scenario_name_ = GetScenarioNameInfo(scenario_name).base_name;
  }

  bool Draw(Application& app) {
    ImGui::IdGuard cid("CreateLevelsPlaylistDialogContent");
    bool did_add = false;
    if (is_open_) {
      if (ImGui::BeginDefaultPopupModal(id_.c_str(), &is_open_)) {
        ImGui::SimpleDropdown("BundlePicker",
                              name_.mutable_bundle_name(),
                              bundle_names_,
                              ImGui::GetFrameHeight() * 9);
        ImGui::SameLine();
        ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());

        ImGui::Spacing();
        if (ImGui::Button("Create playlist")) {
          auto taken_names =
              app.playlist_manager().GetAllRelativeNamesInBundle(name_.bundle_name());
          *name_.mutable_relative_name() = MakeUniqueName(name_.relative_name(), taken_names);
          PlaylistDef def;
          auto* levels = def.mutable_levels();
          levels->set_base_scenario(scenario_name_);
          levels->set_max_level(30);
          app.playlist_manager().UpdatePlaylist(name_.full_name(), def);
          bool saved = app.bundle_manager().SaveDirtyBundles();
          if (saved) {
            app.playlist_manager().SetCurrentPlaylist(name_.full_name());
            app.history_manager().UpdateRecentView(ObjectType::PLAYLIST, name_.full_name());
            app.state().go_to_app_screen = AppScreen::PLAYLISTS;
            did_add = true;
          }
          // TODO: Error popup if failed to save.
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
      bundle_names_ = app.bundle_manager().GetWritableBundleNames();

      name_ = ResourceName::Parse(scenario_name_);
      if (app.bundle_manager().IsBundleReadonly(name_.bundle_name())) {
        *name_.mutable_bundle_name() = kUserBundleName;
      }
    }
    return did_add;
  }

 private:
  bool open_ = false;
  bool is_open_ = false;
  std::string scenario_name_;

  ResourceName name_;
  std::vector<std::string> bundle_names_;
  std::string id_ = "CreateLevelsPlaylistDialog";
};

struct ScenarioDialogs {
  ImGui::ConfirmationDialog<std::string> delete_confirmation_dialog{"DeleteConfirmationDialog"};
  CreateLevelsPlaylistDialog create_levels_playlist_dialog;
  SelectVariationDialog select_variation_dialog =
      SelectVariationDialog::ForScenarios("SelectScenarioVariation");
  // TODO: This should not be in dialogs. Maybe use events to communicate somehow?
  bool update_filtered_scenarios = false;
};

void DrawScenarioRightClickMenu(const char* popup_id,
                                const std::string& scenario_name,
                                ScenarioDialogs* dialogs,
                                Application& app) {
  if (ImGui::BeginPopupContextItem(popup_id)) {
    bool is_readonly = app.bundle_manager().IsBundleReadonly(GetBundleName(scenario_name));
    if (!is_readonly && ImGui::Selectable("Edit")) {
      ScenarioEditorOptions opts;
      opts.scenario_name = scenario_name;
      app.GetCurrentScreen()->PushNextScreen(CreateScenarioEditorScreen(opts, &app));
    }
    if (ImGui::Selectable("Copy")) {
      ScenarioEditorOptions opts;
      opts.scenario_name = scenario_name;
      opts.is_new_copy = true;
      app.GetCurrentScreen()->PushNextScreen(CreateScenarioEditorScreen(opts, &app));
    }
    if (ImGui::Selectable("Select variation")) {
      dialogs->select_variation_dialog.NotifyOpen(scenario_name);
    }
    if (ImGui::Selectable("Remove from recents")) {
      app.history_manager().DeleteRecentView(ObjectType::SCENARIO, scenario_name);
      dialogs->update_filtered_scenarios = true;
    }
    if (ImGui::BeginMenu("Add to")) {
      ImGui::LoopId playlist_loop_id;
      std::string selected_playlist;
      const auto& recent_playlists = app.history_manager().recent_playlists();
      int playlist_count = 0;
      for (int i = 0; i < recent_playlists.size(); ++i) {
        if (playlist_count >= 6) {
          break;
        }
        const std::string& playlist_name = recent_playlists[i];
        auto id = playlist_loop_id.Get("AddToPlaylist");
        auto maybe_playlist = app.playlist_manager().GetPlaylist(playlist_name);
        if (!maybe_playlist.has_value() || maybe_playlist->def().has_levels()) {
          continue;
        }
        playlist_count++;
        if (ImGui::MenuItem(playlist_name.c_str())) {
          selected_playlist = playlist_name;
        }
      }
      if (selected_playlist.size() > 0) {
        app.playlist_manager().AddScenarioToPlaylist(selected_playlist, scenario_name);
        app.bundle_manager().SaveBundle(ResourceName::Parse(selected_playlist).bundle_name());
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Advanced")) {
      if (ImGui::Selectable("View stats")) {
        app.GetCurrentScreen()->PushNextScreen(CreateStatsScreen(
            scenario_name, app.stats_manager().GetLatestRunId(scenario_name), false, &app));
      }
      if (ImGui::Selectable("Copy as reference")) {
        ScenarioEditorOptions opts;
        opts.scenario_name = scenario_name;
        opts.is_new_copy = true;
        opts.copy_as_reference = true;
        app.GetCurrentScreen()->PushNextScreen(CreateScenarioEditorScreen(opts, &app));
      }
      if (ImGui::Selectable("Create levels playlist")) {
        dialogs->create_levels_playlist_dialog.NotifyOpen(scenario_name);
      }
      ImGui::EndMenu();
    }
    if (!is_readonly) {
      ImGui::SpacedSeparator();
      if (ImGui::Selectable("Delete")) {
        std::string base_name = GetScenarioNameInfo(scenario_name).base_name;
        dialogs->delete_confirmation_dialog.NotifyOpen(std::format("Delete \"{}\"?", base_name),
                                                       base_name);
      }
    }
    ImGui::EndPopup();
  }
}

class ScenarioBrowserComponent {
 public:
  explicit ScenarioBrowserComponent(const std::string& id, Application* app) : id_(id), app_(app) {
    auto maybe_initial_view_type = app_->local_store().GetInt(GetViewTypeKey());
    if (maybe_initial_view_type) {
      view_type_ = static_cast<ScenarioViewType>(*maybe_initial_view_type);
    } else {
      view_type_ = ScenarioViewType::ALL;
    }

    search_text_ = app_->local_store().Get(GetSearchTextKey());

    shot_types_.push_back({ShotType::TYPE_NOT_SET, "None"});
    shot_types_.insert(shot_types_.end(), kShotTypes.begin(), kShotTypes.end());

    scenario_types_.push_back({ScenarioDef::TYPE_NOT_SET, "None"});
    scenario_types_.insert(scenario_types_.end(), kScenarioTypes.begin(), kScenarioTypes.end());

    UpdateFilteredScenarios();
    initial_search_text_ = search_text_;
  }

  ~ScenarioBrowserComponent() {
    if (initial_search_text_ != search_text_) {
      app_->local_store().Put(GetSearchTextKey(), search_text_);
    }
  }

  void Reload() {
    UpdateFilteredScenarios();
  }

  void Show(ScenarioDialogs* dialogs) {
    if (dialogs->update_filtered_scenarios) {
      UpdateFilteredScenarios();
      dialogs->update_filtered_scenarios = false;
    }
    float char_x = ImGui::GetDefaultCharSizeX();
    ImGui::IdGuard cid(id_);

    ImGui::Spacing();
    if (ImGui::Button(std::format("{} Scenario", icons::kAdd))) {
      ScenarioEditorOptions opts;
      opts.scenario_name = "";
      opts.is_new_copy = true;
      app_->GetCurrentScreen()->PushNextScreen(CreateScenarioEditorScreen(opts, app_));
    }

    ImGui::SpacedSeparator();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", icons::kFilterList);
    ImGui::SameLine();
    bool view_type_changed = ImGui::ChipSelector("##ScenarioViewType",
                                                 &view_type_,
                                                 {
                                                     {ScenarioViewType::ALL, "All"},
                                                     {ScenarioViewType::RECENT, "Recent"},
                                                     {ScenarioViewType::STARRED, "Starred"},
                                                 });
    if (view_type_changed) {
      app_->local_store().PutInt(GetViewTypeKey(), (int)view_type_);
      UpdateFilteredScenarios();
    }

    if (advanced_filters_open_) {
      ImGui::Indent();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Shot type");
      ImGui::SameLine();
      bool shot_type_changed = ImGui::SimpleTypeDropdown(
          "##ShotTypeFilter", &shot_type_filter_, shot_types_, char_x * 15);
      if (shot_type_changed) {
        UpdateFilteredScenarios();
      }

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Scenario type");
      ImGui::SameLine();
      bool scenario_type_changed = ImGui::SimpleTypeDropdown(
          "##ScenarioTypeFilter", &scenario_type_filter_, scenario_types_, char_x * 12);
      if (scenario_type_changed) {
        UpdateFilteredScenarios();
      }

      if (ImGui::Button("Clear filters")) {
        shot_type_filter_ = ShotType::TYPE_NOT_SET;
        scenario_type_filter_ = ScenarioDef::TYPE_NOT_SET;
        UpdateFilteredScenarios();
        advanced_filters_open_ = false;
      }

      ImGui::Unindent();
      ImGui::Spacing();
    } else {
      ImGui::SameLine(0, char_x);
      if (ImGui::Button("Advanced")) {
        advanced_filters_open_ = true;
      }
      ImGui::HelpTooltip("Filter scenarios using advanced filters like by shot type");
    }

    ImGui::SetNextItemWidth(char_x * 31);
    ImGui::InputTextWithHint("##ScenarioSearchInput", icons::kSearch, &search_text_);
    if (search_text_.size() > 0) {
      ImGui::SameLine();
      if (ImGui::SelectableButton(icons::kClear)) {
        search_text_ = "";
      }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::BeginChild("ScenarioContent")) {
      DrawScenariosTable(dialogs);
    }
    ImGui::EndChild();
  }

  void DrawScenariosTable(ScenarioDialogs* dialogs) {
    if (ShouldUpdateFilteredScenarios()) {
      UpdateFilteredScenarios();
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable("ScenarioTable", 1, flags)) {
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

      ImGuiListClipper clipper;
      clipper.Begin(filtered_scenario_names_.size());

      while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
          ImGui::IdGuard lid("ScenarioItem", i);
          const std::string& scenario_name =
              IsValidIndex(filtered_scenario_names_, i) ? filtered_scenario_names_[i] : "";

          std::optional<ScenarioItem> maybe_scenario =
              app_->scenario_manager().GetScenario(scenario_name);

          ImGui::TableNextRow();

          ImGui::TableNextColumn();
          if (maybe_scenario) {
            /*
          if (ImGui::Button(icons::kPlayArrow)) {
            result->scenario_to_start = scenario_name;
          }
          ImGui::SameLine();
          */
            DrawScenarioItem(*maybe_scenario, dialogs);
          } else {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(scenario_name);
            ImGui::SameLine();
            if (view_type_ == ScenarioViewType::RECENT) {
              if (ImGui::Selectable(
                      icons::kDelete, false, 0, ImVec2(ImGui::GetTextLineHeight(), 0))) {
                app_->history_manager().DeleteRecentView(ObjectType::SCENARIO, scenario_name);
                UpdateFilteredScenarios();
              }
              ImGui::HelpTooltip("Delete from recents");
            }
            if (view_type_ == ScenarioViewType::STARRED) {
              DrawStarItemSelectable(scenario_name);
            }
          }
        }
      }

      ImGui::EndTable();
    }
  }

 private:
  std::string GetViewTypeKey() {
    return std::format("ScenarioViewType_{}", id_);
  }

  std::string GetSearchTextKey() {
    return std::format("ScenarioSearchText_{}", id_);
  }

  void DrawScenarioItem(const ScenarioItem& scenario, ScenarioDialogs* dialogs) {
    if (ImGui::Button(scenario.name)) {
      // If already selected and clicked again, start the scenario. i.e. double click to start
      // TODO: Improve this UX
      if (app_->scenario_manager().GetCurrentScenarioName() == scenario.name) {
        app_->state().scenario_run_option = ScenarioRunOption::START_CURRENT;
      } else {
        app_->scenario_manager().SetCurrentScenario(scenario.name);
      }
    }
    const char* popup_id = "ScenarioItemMenu";
    DrawScenarioRightClickMenu(popup_id, scenario.name, dialogs, *app_);
    ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);

    ImGui::SameLine();
    DrawStarItemSelectable(scenario.name);
  }

  void DrawStarItemSelectable(const std::string& scenario_name) {
    if (app_->labels_manager().IsStarred(ObjectType::SCENARIO, scenario_name)) {
      if (ImGui::Selectable(icons::kStar, false, 0, ImVec2(ImGui::GetTextLineHeight(), 0))) {
        app_->labels_manager().UnstarItem(ObjectType::SCENARIO, scenario_name);
        UpdateFilteredScenarios();
      }
    } else {
      if (ImGui::Selectable(icons::kStarOutline, false, 0, ImVec2(ImGui::GetTextLineHeight(), 0))) {
        app_->labels_manager().StarItem(ObjectType::SCENARIO, scenario_name);
        // UpdateFilteredScenarios(); Is this necessary? You can't click this from the starred
        // list.
      }
    }
  }

  bool ShouldUpdateFilteredScenarios() {
    return handled_search_text_ != search_text_;
  }

  void UpdateFilteredScenarios() {
    filtered_scenario_names_.clear();

    filtered_scenario_names_.reserve(app_->scenario_manager().scenario_names()->size());
    auto search_words = GetSearchWords(search_text_);

    bool has_shot_type_filter = shot_type_filter_ != ShotType::TYPE_NOT_SET;
    bool has_scenario_type_filter = scenario_type_filter_ != ScenarioDef::TYPE_NOT_SET;
    bool has_advanced_filter = has_shot_type_filter || has_scenario_type_filter;

    auto scenario_name_matches = [&](const std::string& scenario_name) {
      // bool search_matches = StringMatchesSearch(scenario_name, search_words);
      bool search_matches = search_text_.empty() || FuzzyMatch(search_text_, scenario_name);
      if (!search_matches) {
        return false;
      }
      if (!has_advanced_filter) {
        return true;
      }
      // Filter by shot type
      auto maybe_def = app_->scenario_manager().GetEvaluatedScenarioDef(scenario_name);
      if (!maybe_def) {
        return false;
      }
      auto maybe_scenario_item = app_->scenario_manager().GetScenario(scenario_name);
      if (!maybe_scenario_item) {
        return false;
      }
      const ScenarioDef& def = *maybe_def;
      const ScenarioDef& unevaluated_def = maybe_scenario_item->unevaluated_def;
      if (has_shot_type_filter && def.shot_type().type_case() != shot_type_filter_) {
        return false;
      }

      if (has_scenario_type_filter) {
        if (scenario_type_filter_ == ScenarioDef::kReferenceDef) {
          if (unevaluated_def.type_case() != ScenarioDef::kReferenceDef) {
            return false;
          }
        } else {
          if (def.type_case() != scenario_type_filter_) {
            return false;
          }
        }
      }

      return true;
    };

    if (view_type_ == ScenarioViewType::STARRED) {
      auto items = app_->labels_manager().ListStarredItems(ObjectType::SCENARIO);
      for (const std::string& scenario_name : items->items) {
        if (scenario_name_matches(scenario_name)) {
          filtered_scenario_names_.push_back(scenario_name);
        }
      }
    } else if (view_type_ == ScenarioViewType::RECENT) {
      for (const std::string& scenario_name : app_->history_manager().recent_scenarios()) {
        if (scenario_name_matches(scenario_name)) {
          filtered_scenario_names_.push_back(scenario_name);
        }
      }
    } else {
      for (const std::string& scenario_name : *app_->scenario_manager().scenario_names()) {
        if (scenario_name_matches(scenario_name)) {
          filtered_scenario_names_.push_back(scenario_name);
        }
      }
    }
    handled_search_text_ = search_text_;
  }

  std::string search_text_;
  std::string initial_search_text_;

  ScenarioViewType view_type_ = ScenarioViewType::ALL;
  std::string handled_search_text_;
  std::vector<std::string> filtered_scenario_names_;

  // If greater than 0 will expand/collapse all. Will be decremented each render loop.
  int expand_all_ = 0;
  int collapse_all_ = 0;
  Application* app_;

  std::string id_;

  ScenarioDef::TypeCase scenario_type_filter_ = ScenarioDef::TYPE_NOT_SET;
  ShotType::TypeCase shot_type_filter_ = ShotType::TYPE_NOT_SET;
  std::vector<std::pair<ShotType::TypeCase, std::string>> shot_types_;
  std::vector<std::pair<ScenarioDef::TypeCase, std::string>> scenario_types_;
  bool advanced_filters_open_ = false;
};

class ScenariosComponentImpl : public ScenariosComponent {
 public:
  ScenariosComponentImpl(Application& app)
      : app_(app), scenario_browser_("ScenarioBrowser", &app) {}

  void Show() override {
    std::optional<std::string> scenario_to_delete =
        dialogs_.delete_confirmation_dialog.Draw("Delete");
    if (scenario_to_delete) {
      auto maybe_scenario = app_.scenario_manager().GetScenario(*scenario_to_delete);
      if (maybe_scenario.has_value()) {
        app_.scenario_manager().DeleteScenario(maybe_scenario->name);
        app_.bundle_manager().SaveDirtyBundles();
        Reload();
      }
    }
    dialogs_.create_levels_playlist_dialog.Draw(app_);

    std::string updated_scenario_variation_name;
    if (dialogs_.select_variation_dialog.Draw(&updated_scenario_variation_name)) {
      app_.scenario_manager().SetCurrentScenario(updated_scenario_variation_name);
      // TODO: Add to history?
    }

    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("ScenarioColumns", 2, flags)) {
      ImGui::TableNextColumn();
      scenario_browser_.Show(&dialogs_);

      ImGui::TableNextColumn();
      DrawCurrentScenarioPanel();

      ImGui::EndTable();
    }
  }

  void Reload() override {
    scenario_browser_.Reload();
  }

 private:
  void DrawCurrentScenarioPanel() {
    ImGui::IdGuard cid("CurrentScenario");
    auto maybe_current_scenario = app_.scenario_manager().GetCurrentScenario();
    if (!maybe_current_scenario) {
      return;
    }
    const ScenarioItem& item = *maybe_current_scenario;
    std::optional<ScenarioDef> evaluated_def =
        app_.scenario_manager().GetEvaluatedScenarioDef(item.name);
    ImGui::Spacing();
    if (ImGui::Button(std::format("{}", icons::kPlayArrow))) {
      app_.state().scenario_run_option = ScenarioRunOption::START_CURRENT;
    }
    ImGui::SameLine();
    ImGui::Text(item.name);

    ImGui::SameLine();
    const char* popup_id = "CurrentScenarioMenu";
    DrawScenarioRightClickMenu(popup_id, item.name, &dialogs_, app_);
    ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
    if (ImGui::SelectableButton(icons::kMoreVert)) {
      ImGui::OpenPopup(popup_id);
    }

    if (evaluated_def) {
      std::vector<std::string> chips;
      auto type_it = kScenarioTypeDisplayNameMap.find(evaluated_def->type_case());
      if (type_it != kScenarioTypeDisplayNameMap.end()) {
        chips.push_back(type_it->second);
      }

      auto shot_type_it = kShotTypeDisplayNameMap.find(evaluated_def->shot_type().type_case());
      if (shot_type_it != kShotTypeDisplayNameMap.end()) {
        chips.push_back(shot_type_it->second);
      }

      if (chips.size() > 0) {
        ImGui::SpacedSeparator();
        // TODO: Use better visual type other than disabled button.
        ImGui::BeginDisabled();
        ImGui::LoopId lid;
        for (int i = 0; i < chips.size(); ++i) {
          auto id = lid.Get();
          if (i > 0) {
            ImGui::SameLine();
          }
          ImGui::Button(chips[i]);
        }
        ImGui::EndDisabled();
      }
    }

    auto stats = app_.stats_manager().GetAggregateStats(item.name);
    if (stats.total_runs > 0) {
      ImGui::IdGuard cid("HighScore");
      ImGui::SpacedSeparator();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("High score");
      ImGui::SameLine();
      if (ImGui::Button(MaybeIntToString(stats.high_score_stats.score))) {
        app_.GetCurrentScreen()->PushNextScreen(
            CreateStatsScreen(item.name, stats.high_score_stats.stats_id, false, &app_));
      }
      ImGui::HelpTooltip("View stats for run.");
      std::string high_score_time = GetHowLongAgoStringFromEpochSeconds(
          stats.high_score_stats.epoch_seconds, GetNowEpochSeconds());
      ImGui::SameLine();
      ImGui::TextFmt("({})", high_score_time);

      std::string last_run_str = GetHowLongAgoStringFromEpochSeconds(
          stats.last_run_stats.epoch_seconds, GetNowEpochSeconds());
      if (stats.total_runs == 1) {
        ImGui::TextFmt("1 run ({})", last_run_str);
      } else {
        ImGui::TextFmt("{} runs ({})", stats.total_runs, last_run_str);
      }
    }

    std::string description = FirstNonEmpty(item.unevaluated_def.description(),
                                            item.unevaluated_def.reference_def().description());
    if (description.size() > 0) {
      ImGui::SpacedSeparator();
      ImGui::TextWrapped(description);
    }

    if (cached_scenario_name_ != item.name) {
      // Calculate "expensive" things only the first time the scenario is switched to.
      matching_playlists_ = app_.playlist_manager().FindPlaylistsContainingScenario(item.name);
      absl::c_sort(*matching_playlists_);
      referencing_scenarios_ = app_.scenario_manager().GetReferencingScenarios(item.name);
      absl::c_sort(referencing_scenarios_);
      cached_scenario_name_ = item.name;
    }

    std::string referenced_scenario = item.unevaluated_def.reference_def().scenario_name();
    if (!referenced_scenario.empty()) {
      ImGui::SpacedSeparator();
      ImGui::IdGuard cid("ReferencedScenario");
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Referenced scenario");
      ImGui::SameLine();
      ImGui::HelpMarker(
          "This scenario references/extends the following scenario. Editing the referenced "
          "scenario will alter the behavior of this scenario.");
      ImGui::SameLine();
      if (ImGui::Button(referenced_scenario)) {
        app_.scenario_manager().SetCurrentScenario(referenced_scenario);
      }
      const char* popup_id = "ReferencedScenarioMenu";
      DrawScenarioRightClickMenu(popup_id, referenced_scenario, &dialogs_, app_);
      ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
    }

    if (!referencing_scenarios_.empty()) {
      ImGui::SpacedSeparator();
      ImGui::Text("Referencing scenarios");
      ImGui::SameLine();
      ImGui::HelpMarker(
          "Scenarios that extend this scenario. Changing this scenario would change the following "
          "ones too.");
      ImGui::Indent();
      ImGui::LoopId loop_id;
      for (const std::string& name : referencing_scenarios_) {
        auto lid = loop_id.Get();
        if (ImGui::Button(name)) {
          app_.scenario_manager().SetCurrentScenario(name);
        }
        const char* popup_id = "ReferencingScenarioMenu";
        DrawScenarioRightClickMenu(popup_id, name, &dialogs_, app_);
        ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
      }
      ImGui::Unindent();
    }

    if (!matching_playlists_->empty()) {
      ImGui::SpacedSeparator();
      ImGui::Text("Related playlists");
      ImGui::Indent();
      for (const std::string& playlist_name : *matching_playlists_) {
        if (ImGui::Button(playlist_name)) {
          app_.playlist_manager().SetCurrentPlaylist(playlist_name);
          app_.state().go_to_app_screen = AppScreen::PLAYLISTS;
        }
      }
      ImGui::Unindent();
    }
  }

  Application& app_;
  ScenarioBrowserComponent scenario_browser_;

  // The scenario which cached information was stored for
  std::string cached_scenario_name_;
  std::optional<std::vector<std::string>> matching_playlists_;
  std::vector<std::string> referencing_scenarios_;

  ScenarioDialogs dialogs_;
};

}  // namespace

std::unique_ptr<ScenariosComponent> CreateScenariosComponent(Application& app) {
  return std::make_unique<ScenariosComponentImpl>(app);
}

}  // namespace aim

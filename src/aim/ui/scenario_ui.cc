#include "scenario_ui.h"

#include <algorithm>

#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "aim/common/search.h"
#include "aim/common/times.h"
#include "aim/core/application.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/stats_manager.h"
#include "aim/editor/scenario_editor_screen.h"
#include "aim/ui/stats_screen.h"
#include "imgui.h"

namespace aim {
namespace {

enum class ScenarioViewType : int {
  RECENT = 1,
  STARRED = 2,
  ALL = 3,
};

void DrawScenarioRightClickMenu(const char* popup_id,
                                const std::string& scenario_name,
                                ImGui::ConfirmationDialog<std::string>* delete_confirmation_dialog,
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
    if (ImGui::BeginMenu("Add to")) {
      ImGui::LoopId playlist_loop_id;
      std::string selected_playlist;
      for (auto& playlist_name : app.history_manager().recent_playlists()) {
        auto id = playlist_loop_id.Get();
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
    /*
    if (ImGui::BeginMenu("Add copy to")) {
      ImGui::LoopId playlist_loop_id;
      std::string selected_playlist;
      for (auto& playlist_name : app.history_manager().recent_playlists()) {
        auto id = playlist_loop_id.Get();
        if (ImGui::MenuItem(playlist_name.c_str())) {
          selected_playlist = playlist_name;
          ScenarioEditorOptions opts;
          opts.scenario_name = scenario_name;
          opts.is_new_copy = true;
          opts.add_to_playlist = playlist_name;
          opts.force_bundle_name = ResourceName::Parse(playlist_name).bundle_name();
          app.GetCurrentScreen()->PushNextScreen(CreateScenarioEditorScreen(opts, &app));
        }
      }
      ImGui::EndMenu();
    }
    */
    if (!is_readonly) {
      ImGui::SpacedSeparator();
      if (ImGui::Selectable("Delete")) {
        delete_confirmation_dialog->NotifyOpen(std::format("Delete \"{}\"?", scenario_name),
                                               scenario_name);
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

  void Show(ImGui::ConfirmationDialog<std::string>* delete_confirmation_dialog) {
    ImGui::IdGuard cid(id_);

    ImGui::Spacing();
    if (ImGui::Button(std::format("{} Add scenario", icons::kAdd))) {
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

    ImVec2 char_size = ImGui::CalcTextSize("A");
    ImGui::SetNextItemWidth(char_size.x * 30);
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
      DrawScenariosTable(delete_confirmation_dialog);
    }
    ImGui::EndChild();
  }

  void DrawScenariosTable(ImGui::ConfirmationDialog<std::string>* delete_confirmation_dialog) {
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
            DrawScenarioItem(*maybe_scenario, delete_confirmation_dialog);
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

  void DrawScenarioItem(const ScenarioItem& scenario,
                        ImGui::ConfirmationDialog<std::string>* delete_confirmation_dialog) {
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
    DrawScenarioRightClickMenu(popup_id, scenario.name, delete_confirmation_dialog, *app_);
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
    if (view_type_ == ScenarioViewType::STARRED) {
      auto items = app_->labels_manager().ListStarredItems(ObjectType::SCENARIO);
      for (const std::string& scenario_name : items->items) {
        if (StringMatchesSearch(scenario_name, search_words)) {
          filtered_scenario_names_.push_back(scenario_name);
        }
      }
    } else if (view_type_ == ScenarioViewType::RECENT) {
      for (const std::string& scenario_name : app_->history_manager().recent_scenarios()) {
        if (StringMatchesSearch(scenario_name, search_words)) {
          filtered_scenario_names_.push_back(scenario_name);
        }
      }
    } else {
      for (const std::string& scenario_name : *app_->scenario_manager().scenario_names()) {
        if (StringMatchesSearch(scenario_name, search_words)) {
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
};

class ScenariosComponentImpl : public ScenariosComponent {
 public:
  ScenariosComponentImpl(Application& app)
      : app_(app), scenario_browser_("ScenarioBrowser", &app) {}

  void Show() override {
    delete_confirmation_dialog_.Draw("Delete", [=](const std::string& scenario_name) {
      auto maybe_scenario = app_.scenario_manager().GetScenario(scenario_name);
      if (maybe_scenario.has_value()) {
        app_.scenario_manager().DeleteScenario(maybe_scenario->name);
        app_.bundle_manager().SaveDirtyBundles();
        Reload();
      }
    });
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("ScenarioColumns", 2, flags)) {
      ImGui::TableNextColumn();
      scenario_browser_.Show(&delete_confirmation_dialog_);

      ImGui::TableNextColumn();
      DrawCurrentScenarioComponent("CurrentScenarioComponent", app_);

      ImGui::EndTable();
    }
  }

  void Reload() override {
    scenario_browser_.Reload();
  }

 private:
  void DrawCurrentScenarioComponent(const std::string& id, Application& app) {
    ImGui::IdGuard cid(id);
    auto maybe_current_scenario = app.scenario_manager().GetCurrentScenario();
    if (!maybe_current_scenario) {
      return;
    }
    const ScenarioItem& item = *maybe_current_scenario;
    ImGui::Text(item.name);
    ImGui::SameLine();
    if (ImGui::Button(icons::kEdit)) {
      ScenarioEditorOptions opts;
      opts.scenario_name = item.name;
      app.GetCurrentScreen()->PushNextScreen(CreateScenarioEditorScreen(opts, &app));
    }

    /*
    ImGui::SameLine();
    if (ImGui::SelectableButton(icons::kMoreVert)) {
    }
    */

    if (ImGui::Button(std::format("{} Play", icons::kPlayArrow))) {
      app.state().scenario_run_option = ScenarioRunOption::START_CURRENT;
    }
    if (app.scenario_manager().has_running_scenario()) {
      ImGui::SameLine();
      if (ImGui::Button("Resume")) {
        app.state().scenario_run_option = ScenarioRunOption::RESUME_CURRENT;
      }
    }
    std::string description = item.unevaluated_def.description();
    if (description.size() > 0) {
      if (ImGui::TreeNode("Description")) {
        ImGui::Text(description);
        ImGui::TreePop();
      }
    }
  }

  Application& app_;
  ScenarioBrowserComponent scenario_browser_;
  ImGui::ConfirmationDialog<std::string> delete_confirmation_dialog_{"DeleteConfirmationDialog"};
};

}  // namespace

std::unique_ptr<ScenariosComponent> CreateScenariosComponent(Application& app) {
  return std::make_unique<ScenariosComponentImpl>(app);
}

}  // namespace aim

#include "scenario_ui.h"

#include <algorithm>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/common/times.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/stats_manager.h"
#include "aim/editor/scenario_editor_screen.h"
#include "imgui.h"

namespace aim {
namespace {

enum class ScenarioViewType : int {
  RECENT = 1,
  STARRED = 2,
  ALL = 3,
};

class ScenarioBrowserComponentImpl : public ScenarioBrowserComponent {
 public:
  explicit ScenarioBrowserComponentImpl(ScenarioBrowserType type,
                                        const std::string& id,
                                        Application* app)
      : type_(type), id_(id), app_(app) {
    if (type_ == ScenarioBrowserType::FULL) {
      auto maybe_initial_view_type = app_->local_store().GetInt(GetViewTypeKey());
      if (maybe_initial_view_type) {
        view_type_ = static_cast<ScenarioViewType>(*maybe_initial_view_type);
      } else {
        view_type_ = ScenarioViewType::ALL;
      }

      search_text_ = app_->local_store().Get(GetSearchTextKey());
    }
    UpdateFilteredScenarios();
    initial_search_text_ = search_text_;
  }

  ~ScenarioBrowserComponentImpl() {
    if (initial_search_text_ != search_text_) {
      app_->local_store().Put(GetSearchTextKey(), search_text_);
    }
  }

  void Reload() override {
    UpdateFilteredScenarios();
  }

  void Show(ScenarioBrowserResult* result) override {
    ImGui::IdGuard cid(id_);

    delete_confirmation_dialog_.Draw("Delete", [=](const std::string& scenario_name) {
      auto maybe_scenario = app_->scenario_manager().GetScenario(scenario_name);
      if (maybe_scenario.has_value()) {
        app_->scenario_manager().DeleteScenario(maybe_scenario->name);
        app_->bundle_manager().SaveDirtyBundles();
        result->reload_scenarios = true;
      }
    });

    if (type_ == ScenarioBrowserType::QUICK_ACCESS) {
      if (ImGui::BeginChild("ScenarioContent")) {
        DrawScenariosTable(result);
      }
      ImGui::EndChild();
      return;
    }

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
      if (ImGui::Button(icons::kCancel)) {
        search_text_ = "";
      }
    }


    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::BeginChild("ScenarioContent")) {
      DrawScenariosTable(result);
    }
    ImGui::EndChild();
  }

  void DrawScenariosTable(ScenarioBrowserResult* result) {
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
            DrawScenarioItem(*maybe_scenario, result);
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

  void DrawScenarioItem(const ScenarioItem& scenario, ScenarioBrowserResult* result) {
    if (ImGui::Button(scenario.name)) {
      if (app_->scenario_manager().GetCurrentScenarioName() == scenario.name) {
        result->scenario_to_start = scenario.name;
      } else {
        app_->scenario_manager().SetCurrentScenario(scenario.name);
      }
    }
    const char* popup_id = "ScenarioItemMenu";
    if (ImGui::BeginPopupContextItem(popup_id)) {
      if (ImGui::BeginMenu("Add to")) {
        ImGui::LoopId playlist_loop_id;
        std::string selected_playlist;
        for (auto& playlist_name : app_->history_manager().recent_playlists()) {
          auto id = playlist_loop_id.Get();
          if (ImGui::MenuItem(playlist_name.c_str())) {
            selected_playlist = playlist_name;
          }
        }
        if (selected_playlist.size() > 0) {
          app_->playlist_manager().AddScenarioToPlaylist(selected_playlist, scenario.name);
          app_->bundle_manager().SaveBundle(ResourceName::Parse(selected_playlist).bundle_name());
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Add copy to")) {
        ImGui::LoopId playlist_loop_id;
        std::string selected_playlist;
        for (auto& playlist_name : app_->history_manager().recent_playlists()) {
          auto id = playlist_loop_id.Get();
          if (ImGui::MenuItem(playlist_name.c_str())) {
            selected_playlist = playlist_name;
            ScenarioEditorOptions opts;
            opts.scenario_name = scenario.name;
            opts.is_new_copy = true;
            opts.add_to_playlist = playlist_name;
            opts.force_bundle_name = ResourceName::Parse(playlist_name).bundle_name();
            app_->GetCurrentScreen()->PushNextScreen(CreateScenarioEditorScreen(opts, app_));
          }
        }
        ImGui::EndMenu();
      }
      if (ImGui::Selectable("Edit")) {
        result->scenario_to_edit = scenario.name;
      }
      if (ImGui::Selectable("Copy")) {
        result->scenario_to_edit_copy = scenario.name;
      }
      if (ImGui::Selectable("Delete")) {
        delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", scenario.name),
                                               scenario.name);
      }
      if (ImGui::BeginMenu("Advanced")) {
        if (ImGui::MenuItem("View latest run")) {
          result->scenario_stats_to_view = scenario.name;
          result->run_id = app_->stats_manager().GetLatestRunId(scenario.name);
        }
        if (ImGui::MenuItem("Reload")) {
          result->reload_scenarios = true;
        }
        ImGui::EndMenu();
      }
      ImGui::EndPopup();
    }
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
        // UpdateFilteredScenarios(); Is this necessary? You can't click this from the starred list.
      }
    }
  }

  bool ShouldUpdateFilteredScenarios() {
    return handled_search_text_ != search_text_;
  }

  void GetQuickAccessScenarios(std::vector<std::string>* scenario_names) {
    int limit = 25;
    int max_recent_maybe_starred = 8;
    scenario_names->reserve(limit);

    auto starred_items = app_->labels_manager().ListStarredItems(ObjectType::SCENARIO);
    auto recent_scenarios = app_->history_manager().recent_scenarios();

    max_recent_maybe_starred =
        std::max<int>(max_recent_maybe_starred, limit - starred_items->items.size());

    std::unordered_set<std::string> added_scenario_names;

    for (const std::string& scenario_name : recent_scenarios) {
      if (scenario_names->size() >= limit) {
        break;
      }
      bool only_starred = scenario_names->size() >= max_recent_maybe_starred;

      auto scenario = app_->scenario_manager().GetScenario(scenario_name);
      if (scenario.has_value()) {
        bool add = !only_starred || starred_items->item_set.contains(scenario_name);
        if (add) {
          added_scenario_names.insert(scenario_name);
          scenario_names->push_back(scenario_name);
        }
      }
    }
  }

  void UpdateFilteredScenarios() {
    filtered_scenario_names_.clear();
    if (type_ == ScenarioBrowserType::QUICK_ACCESS) {
      GetQuickAccessScenarios(&filtered_scenario_names_);
      return;
    }

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

  ScenarioBrowserType type_;
  std::string search_text_;
  std::string initial_search_text_;

  ScenarioViewType view_type_ = ScenarioViewType::ALL;
  std::string handled_search_text_;
  std::vector<std::string> filtered_scenario_names_;

  ImGui::ConfirmationDialog<std::string> delete_confirmation_dialog_{"DeleteConfirmationDialog"};

  // If greater than 0 will expand/collapse all. Will be decremented each render loop.
  int expand_all_ = 0;
  int collapse_all_ = 0;
  Application* app_;

  std::string id_;

  ScenarioDef::TypeCase scenario_type_filter_ = ScenarioDef::TYPE_NOT_SET;
};

}  // namespace

std::unique_ptr<ScenarioBrowserComponent> CreateScenarioBrowserComponent(const std::string& id,
                                                                         ScenarioBrowserType type,
                                                                         Application* app) {
  return std::make_unique<ScenarioBrowserComponentImpl>(type, id, app);
}

}  // namespace aim

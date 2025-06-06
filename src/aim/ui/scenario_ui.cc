#include "scenario_ui.h"

#include <imgui.h>

#include <algorithm>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/common/times.h"

namespace aim {
namespace {

class ScenarioBrowserComponentImpl : public UiComponent, public ScenarioBrowserComponent {
 public:
  explicit ScenarioBrowserComponentImpl(Application* app) : UiComponent(app) {}

  void Show(ScenarioBrowserType type, ScenarioBrowserResult* result) override {
    auto cid = GetComponentIdGuard();

    delete_confirmation_dialog_.Draw("Delete", [=](const std::string& scenario_id) {
      auto maybe_scenario = app_->scenario_manager().GetScenario(scenario_id);
      if (maybe_scenario.has_value()) {
        app_->scenario_manager().DeleteScenario(maybe_scenario->name);
        result->reload_scenarios = true;
      }
    });

    ImVec2 char_size = ImGui::CalcTextSize("A");
    ImGui::SetNextItemWidth(char_size.x * 30);
    ImGui::InputTextWithHint("##ScenarioSearchInput", kIconSearch, &search_text_);
    if (search_text_.size() > 0) {
      ImGui::SameLine();
      if (ImGui::Button(kIconCancel)) {
        search_text_ = "";
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(kIconRefresh)) {
      result->reload_scenarios = true;
    }
    ImGui::HelpTooltip("Reload scenarios from disk.");

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::BeginChild("ScenarioContent")) {
      auto search_words = GetSearchWords(search_text_);
      if (search_words.size() > 0) {
        expand_all_ = 2;
      }
      if (type == ScenarioBrowserType::RECENT) {
        PlaylistRun* current_playlist_run = app_->playlist_manager().GetCurrentRun();
        ImGui::LoopId loop_id;
        for (const std::string& scenario_id : app_->history_manager().recent_scenario_ids()) {
          auto lid = loop_id.Get();
          auto scenario = app_->scenario_manager().GetScenario(scenario_id);
          if (scenario.has_value()) {
            DrawScenarioListItem(*scenario, search_words, current_playlist_run, result);
          }
        }
      } else {
        DrawScenarioNodes(app_->scenario_manager().scenario_nodes(), search_words, result);
      }
    }
    ImGui::EndChild();
    if (expand_all_ > 0) {
      expand_all_--;
    }
    if (collapse_all_ > 0) {
      collapse_all_--;
    }
  }

 private:
  void CopyScenario(const ScenarioItem& item) {
    std::vector<std::string> taken_names =
        app_->scenario_manager().GetAllRelativeNamesInBundle(item.name.bundle_name());
    std::string final_name = MakeUniqueName(item.name.relative_name() + " Copy", taken_names);
    app_->scenario_manager().SaveScenario(ResourceName(item.name.bundle_name(), final_name),
                                          item.def);
  }

  void DrawScenarioNodes(const std::vector<std::unique_ptr<ScenarioNode>>& nodes,
                         const std::vector<std::string>& search_words,
                         ScenarioBrowserResult* result) {
    PlaylistRun* current_playlist_run = app_->playlist_manager().GetCurrentRun();
    ImGui::LoopId loop_id;
    for (auto& node : nodes) {
      auto id = loop_id.Get("ScenarioNodeItem");
      if (node->scenario.has_value()) {
        DrawScenarioListItem(*node->scenario, search_words, current_playlist_run, result);
      } else {
        if (expand_all_ > 0) {
          ImGui::SetNextItemOpen(true);
        }
        if (collapse_all_ > 0) {
          ImGui::SetNextItemOpen(false);
        }
        bool node_opened = ImGui::TreeNodeEx(
            node->name.c_str(), node->name == "AF" ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        const char* popup_id = "ScenarioBundleMenu";
        if (ImGui::BeginPopupContextItem(popup_id)) {
          if (ImGui::Selectable("Collapse all")) {
            collapse_all_ = 2;
          }
          if (ImGui::Selectable("Expand all")) {
            expand_all_ = 2;
          }
          ImGui::EndPopup();
        }
        ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
        if (node_opened) {
          DrawScenarioNodes(node->child_nodes, search_words, result);
          ImGui::TreePop();
        }
      }
    }
  }

  void DrawScenarioListItem(const ScenarioItem& scenario,
                            const std::vector<std::string>& search_words,
                            PlaylistRun* current_playlist_run,
                            ScenarioBrowserResult* result) {
    if (!StringMatchesSearch(scenario.id(), search_words)) {
      return;
    }
    auto current_scenario = app_->scenario_manager().GetCurrentScenario();
    std::string current_scenario_id = current_scenario ? current_scenario->id() : "";
    if (ImGui::Selectable(scenario.id().c_str(),
                          current_scenario_id == scenario.id(),
                          ImGuiSelectableFlags_AllowDoubleClick)) {
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        result->scenario_to_start = scenario.id();
      } else {
        app_->scenario_manager().SetCurrentScenario(scenario.id());
      }
    }
    const char* popup_id = "ScenarioItemMenu";
    if (ImGui::BeginPopupContextItem(popup_id)) {
      if (current_playlist_run != nullptr) {
        std::string playlist_name = current_playlist_run->playlist.name.full_name();
        std::string add_text = std::format("Add to \"{}\"", playlist_name);
        if (ImGui::Selectable(add_text.c_str())) {
          app_->playlist_manager().AddScenarioToPlaylist(playlist_name, scenario.id());
        }
      }
      if (ImGui::BeginMenu("Add to")) {
        ImGui::LoopId playlist_loop_id;
        std::string selected_playlist;
        int playlist_count = 0;
        for (auto& playlist_name : app_->history_manager().recent_playlists()) {
          auto id = playlist_loop_id.Get();
          if (playlist_count < 10) {
            if (ImGui::MenuItem(playlist_name.c_str())) {
              selected_playlist = playlist_name;
            }
          }
          playlist_count++;
        }
        if (selected_playlist.size() > 0) {
          app_->playlist_manager().AddScenarioToPlaylist(selected_playlist, scenario.id());
        }
        ImGui::EndMenu();
      }
      if (ImGui::Selectable("Edit")) {
        result->scenario_to_edit = scenario.id();
      }
      if (ImGui::Selectable("Edit new copy")) {
        result->scenario_to_edit_copy = scenario.id();
      }
      if (ImGui::Selectable("Quick copy")) {
        CopyScenario(scenario);
        result->reload_scenarios = true;
      }
      if (ImGui::Selectable("View latest run")) {
        result->scenario_stats_to_view = scenario.id();
        result->run_id = app_->stats_manager().GetLatestRunId(scenario.id());
      }
      if (ImGui::Selectable("Generate levels")) {
        app_->scenario_manager().GenerateScenarioLevels(
            scenario.id(), scenario.unevaluated_def.overrides(), 5);
        result->reload_scenarios = true;
      }
      if (ImGui::Selectable("Delete")) {
        delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", scenario.id()),
                                               scenario.id());
      }
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
  }

  std::string search_text_;

  ImGui::ConfirmationDialog<std::string> delete_confirmation_dialog_{"DeleteConfirmationDialog"};

  // If greater than 0 will expand/collapse all. Will be decremented each render loop.
  int expand_all_ = 0;
  int collapse_all_ = 0;
};

}  // namespace

std::unique_ptr<ScenarioBrowserComponent> CreateScenarioBrowserComponent(Application* app) {
  return std::make_unique<ScenarioBrowserComponentImpl>(app);
}

}  // namespace aim

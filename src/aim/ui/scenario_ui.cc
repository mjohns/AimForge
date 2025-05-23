#include "scenario_ui.h"

#include <imgui.h>

#include <algorithm>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/common/times.h"

namespace aim {
namespace {

const char* kDeleteConfirmationPopup = "DELETE_CONFIRMATION_POPUP";

std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name,
                                                     Application* app) {
  std::vector<std::string> names;
  for (const ScenarioItem& s : app->scenario_manager()->scenarios()) {
    if (s.name.bundle_name() == bundle_name) {
      names.push_back(s.name.relative_name());
    }
  }
  return names;
}

class ScenarioBrowserComponentImpl : public UiComponent, public ScenarioBrowserComponent {
 public:
  explicit ScenarioBrowserComponentImpl(Application* app) : UiComponent(app) {}

  void Show(ScenarioBrowserType type, ScenarioBrowserResult* result) override {
    auto cid = GetComponentIdGuard();

    delete_confirmation_dialog_.Draw("Delete", [=](const std::string& scenario_id) {
      auto maybe_scenario = app_->scenario_manager()->GetScenario(scenario_id);
      if (maybe_scenario.has_value()) {
        app_->scenario_manager()->DeleteScenario(maybe_scenario->name);
        result->reload_scenarios = true;
      }
    });

    ImVec2 char_size = ImGui::CalcTextSize("A");
    ImGui::SetNextItemWidth(char_size.x * 30);
    ImGui::InputTextWithHint("##ScenarioSearchInput", "Search..", &search_text_);
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      result->reload_scenarios = true;
      recent_scenario_load_time_micros_ = 0;
    }

    ImGui::Spacing();
    ImGui::Spacing();

    auto search_words = GetSearchWords(search_text_);
    if (type == ScenarioBrowserType::RECENT) {
      MaybeLoadRecentScenarioIds();
      PlaylistRun* current_playlist_run = app_->playlist_manager()->GetCurrentRun();
      ImGui::LoopId loop_id;
      for (const std::string& scenario_id : recent_scenario_ids_) {
        auto id = loop_id.Get();
        auto scenario = app_->scenario_manager()->GetScenario(scenario_id);
        if (scenario.has_value()) {
          DrawScenarioListItem(*scenario, search_words, current_playlist_run, result);
        }
      }
      return;
    }

    DrawScenarioNodes(app_->scenario_manager()->scenario_nodes(), search_words, result);
  }

 private:
  void CopyScenario(const ScenarioItem& item) {
    auto& mgr = *app_->scenario_manager();
    std::vector<std::string> taken_names =
        GetAllRelativeNamesInBundle(item.name.bundle_name(), app_);
    std::string final_name = MakeUniqueName(item.name.relative_name() + " Copy", taken_names);
    mgr.SaveScenario(ResourceName(item.name.bundle_name(), final_name), item.def);
  }

  void DrawScenarioNodes(const std::vector<std::unique_ptr<ScenarioNode>>& nodes,
                         const std::vector<std::string>& search_words,
                         ScenarioBrowserResult* result) {
    PlaylistRun* current_playlist_run = app_->playlist_manager()->GetCurrentRun();
    ImGui::LoopId loop_id;
    for (auto& node : nodes) {
      auto id = loop_id.Get();
      if (node->scenario.has_value()) {
        DrawScenarioListItem(*node->scenario, search_words, current_playlist_run, result);
      }
    }
    for (auto& node : nodes) {
      if (!node->scenario.has_value()) {
        bool node_opened = ImGui::TreeNodeEx(node->name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
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
    if (ImGui::Button(scenario.id().c_str())) {
      result->scenario_to_start = scenario.id();
    }
    if (ImGui::BeginPopupContextItem("scenario_item_menu")) {
      if (current_playlist_run != nullptr) {
        std::string playlist_name = current_playlist_run->playlist.name.full_name();
        std::string add_text = std::format("Add to \"{}\"", playlist_name);
        if (ImGui::Selectable(add_text.c_str())) {
          app_->playlist_manager()->AddScenarioToPlaylist(playlist_name, scenario.id());
        }
      }
      if (ImGui::Selectable("Edit")) {
        result->scenario_to_edit = scenario.id();
      }
      if (ImGui::Selectable("Edit copy")) {
        result->scenario_to_edit_copy = scenario.id();
      }
      if (ImGui::Selectable("Copy")) {
        CopyScenario(scenario);
        result->reload_scenarios = true;
      }
      if (ImGui::Selectable("Delete")) {
        delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", scenario.id()),
                                               scenario.id());
      }
      /*
      if (ImGui::Selectable("Open file")) {
        app_->scenario_manager()->OpenFile(node->scenario->name);
      }
      */
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick("scenario_item_menu", ImGuiPopupFlags_MouseButtonRight);
  }

  void MaybeLoadRecentScenarioIds() {
    u64 now_micros = GetNowMicros();
    float delta_seconds = (now_micros - recent_scenario_load_time_micros_) / 1000000.0;
    if (delta_seconds > 5) {
      recent_scenario_load_time_micros_ = now_micros;
      auto recent_scenarios = app_->history_db()->GetRecentViews(RecentViewType::SCENARIO, 50);
      recent_scenario_ids_.clear();
      for (auto& s : recent_scenarios) {
        if (!VectorContains(recent_scenario_ids_, s.id)) {
          recent_scenario_ids_.push_back(s.id);
        }
      }
    }
  }

  std::string search_text_;

  ImGui::ConfirmationDialog<std::string> delete_confirmation_dialog_{"DeleteConfirmationDialog"};

  std::vector<std::string> recent_scenario_ids_;
  u64 recent_scenario_load_time_micros_ = 0;
};

}  // namespace

std::unique_ptr<ScenarioBrowserComponent> CreateScenarioBrowserComponent(Application* app) {
  return std::make_unique<ScenarioBrowserComponentImpl>(app);
}

}  // namespace aim

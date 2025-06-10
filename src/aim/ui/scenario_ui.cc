#include "scenario_ui.h"

#include <imgui.h>

#include <algorithm>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/common/times.h"

namespace aim {
namespace {

enum ScenarioViewType {
  RECENT,
  ALL,
};

class ScenarioBrowserComponentImpl : public ScenarioBrowserComponent {
 public:
  explicit ScenarioBrowserComponentImpl(Application* app) : app_(app) {
    UpdateFilteredScenarios();
  }

  void Reload() override {
    UpdateFilteredScenarios();
  }

  void Show(ScenarioBrowserType type, ScenarioBrowserResult* result) override {
    ImGui::IdGuard cid("ScenarioBrowserComponent");

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

    ImGui::SetNextItemWidth(char_size.x * 8);
    bool view_type_changed = ImGui::SimpleTypeDropdown("##PlaylistViewType",
                                                       &view_type_,
                                                       {
                                                           {ScenarioViewType::RECENT, "Recent"},
                                                           {ScenarioViewType::ALL, "All"},
                                                       });
    if (view_type_changed) {
      UpdateFilteredScenarios();
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
      clipper.Begin(filtered_scenario_ids_.size());

      while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
          ImGui::IdGuard lid("ScenarioItem", i);
          const std::string& scenario_id =
              IsValidIndex(filtered_scenario_ids_, i) ? filtered_scenario_ids_[i] : "";

          std::optional<ScenarioItem> maybe_scenario =
              app_->scenario_manager().GetScenario(scenario_id);

          ImGui::TableNextRow();

          ImGui::TableNextColumn();
          if (maybe_scenario) {
            /*
          if (ImGui::Button(kIconPlayArrow)) {
            result->scenario_to_start = scenario_id;
          }
          ImGui::SameLine();
          */
            DrawScenarioItem(*maybe_scenario, result);
          } else {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(scenario_id);
          }
        }
      }

      ImGui::EndTable();
    }
  }

 private:
  void DrawScenarioItem(const ScenarioItem& scenario, ScenarioBrowserResult* result) {
    auto current_scenario = app_->scenario_manager().GetCurrentScenario();
    std::string current_scenario_id = current_scenario ? current_scenario->id() : "";
    if (ImGui::Button(scenario.id())) {
      if (app_->scenario_manager().GetCurrentScenarioId() == scenario.id()) {
        result->scenario_to_start = scenario.id();
      } else {
        app_->scenario_manager().SetCurrentScenario(scenario.id());
      }
    }
    const char* popup_id = "ScenarioItemMenu";
    if (ImGui::BeginPopupContextItem(popup_id)) {
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
      if (ImGui::Selectable("Copy")) {
        result->scenario_to_edit_copy = scenario.id();
      }
      if (ImGui::Selectable("Delete")) {
        delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", scenario.id()),
                                               scenario.id());
      }
      if (ImGui::BeginMenu("Advanced")) {
        if (ImGui::MenuItem("View latest run")) {
          result->scenario_stats_to_view = scenario.id();
          result->run_id = app_->stats_manager().GetLatestRunId(scenario.id());
        }
        if (ImGui::MenuItem("Generate levels")) {
          app_->scenario_manager().GenerateScenarioLevels(
              scenario.id(), scenario.unevaluated_def.overrides(), 5);
          result->reload_scenarios = true;
        }
        ImGui::EndMenu();
      }
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
  }

  bool ShouldUpdateFilteredScenarios() {
    return handled_search_text_ != search_text_;
  }

  void UpdateFilteredScenarios() {
    auto search_words = GetSearchWords(search_text_);
    filtered_scenario_ids_.clear();
    filtered_scenario_ids_.reserve(app_->scenario_manager().scenarios().size());

    if (view_type_ == ScenarioViewType::ALL) {
      for (const ScenarioItem& scenario : app_->scenario_manager().scenarios()) {
        if (StringMatchesSearch(scenario.id(), search_words)) {
          filtered_scenario_ids_.push_back(scenario.id());
        }
      }
    } else {
      for (const std::string& scenario_id : app_->history_manager().recent_scenario_ids()) {
        if (StringMatchesSearch(scenario_id, search_words)) {
          filtered_scenario_ids_.push_back(scenario_id);
        }
      }
    }
    handled_search_text_ = search_text_;
  }

  std::string GetCurrentMostRecentScenarioId() {
    const auto& ids = app_->history_manager().recent_scenario_ids();
    return ids.size() > 0 ? ids[0] : "";
  }

  std::string search_text_;

  ScenarioViewType view_type_ = ScenarioViewType::ALL;
  std::string handled_search_text_;
  std::vector<std::string> filtered_scenario_ids_;

  ImGui::ConfirmationDialog<std::string> delete_confirmation_dialog_{"DeleteConfirmationDialog"};

  // If greater than 0 will expand/collapse all. Will be decremented each render loop.
  int expand_all_ = 0;
  int collapse_all_ = 0;
  Application* app_;
};

}  // namespace

std::unique_ptr<ScenarioBrowserComponent> CreateScenarioBrowserComponent(Application* app) {
  return std::make_unique<ScenarioBrowserComponentImpl>(app);
}

}  // namespace aim

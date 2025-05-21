#include "scenario_ui.h"

#include <imgui.h>

#include <algorithm>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"

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

  void Show(ScenarioBrowserResult* result) override {
    auto cid = GetComponentIdGuard();

    DrawDeleteConfirmationPopup(result);

    ImVec2 char_size = ImGui::CalcTextSize("A");
    ImGui::SetNextItemWidth(char_size.x * 30);
    ImGui::InputTextWithHint("##ScenarioSearchInput", "Search..", &search_text_);
    ImGui::SameLine();
    if (ImGui::Button("Reload Scenarios")) {
      result->reload_scenarios = true;
    }
    ImGui::Spacing();
    ImGui::Spacing();

    auto search_words = GetSearchWords(search_text_);
    DrawScenarioNodes(app_->scenario_manager()->scenario_nodes(), search_words, result);
  }

 private:
  void DrawDeleteConfirmationPopup(ScenarioBrowserResult* result) {
    bool show_popup = scenario_to_maybe_delete_.size() > 0;
    if (show_popup) {
      ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                              ImGuiCond_Appearing,
                              ImVec2(0.5f, 0.5f));  // Center the popup
      if (ImGui::BeginPopupModal(kDeleteConfirmationPopup,
                                 &show_popup,
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextFmt("Delete \"{}\"?", scenario_to_maybe_delete_);

        float button_width = ImGui::CalcTextSize("OK").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - button_width) * 0.5f);

        if (ImGui::Button("Delete")) {
          auto maybe_scenario = app_->scenario_manager()->GetScenario(scenario_to_maybe_delete_);
          if (maybe_scenario.has_value()) {
            app_->scenario_manager()->DeleteScenario(maybe_scenario->name);
            result->reload_scenarios = true;
          }
          scenario_to_maybe_delete_ = "";
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
    if (open_delete_confirmation_popup_) {
      ImGui::OpenPopup(kDeleteConfirmationPopup);
      open_delete_confirmation_popup_ = false;
    }
  }

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
      if (node->scenario.has_value() && StringMatchesSearch(node->scenario->id(), search_words)) {
        if (ImGui::Button(node->scenario->id().c_str())) {
          result->scenario_to_start = node->name;
        }
        if (ImGui::BeginPopupContextItem("scenario_item_menu")) {
          if (current_playlist_run != nullptr) {
            std::string playlist_name = current_playlist_run->playlist.name.full_name();
            std::string add_text = std::format("Add to \"{}\"", playlist_name);
            if (ImGui::Selectable(add_text.c_str())) {
              app_->playlist_manager()->AddScenarioToPlaylist(playlist_name, node->name);
            }
          }
          if (ImGui::Selectable("Edit")) {
            result->scenario_to_edit = node->name;
          }
          if (ImGui::Selectable("Copy")) {
            CopyScenario(*node->scenario);
            result->reload_scenarios = true;
          }
          if (ImGui::Selectable("Delete")) {
            scenario_to_maybe_delete_ = node->name;
            open_delete_confirmation_popup_ = true;
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

  std::string search_text_;

  bool open_delete_confirmation_popup_ = false;
  std::string scenario_to_maybe_delete_;
};

}  // namespace

std::unique_ptr<ScenarioBrowserComponent> CreateScenarioBrowserComponent(Application* app) {
  return std::make_unique<ScenarioBrowserComponentImpl>(app);
}

}  // namespace aim

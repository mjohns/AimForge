#include "scenario_ui.h"

#include <imgui.h>

#include <algorithm>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"

namespace aim {
namespace {

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
            app_->scenario_manager()->DeleteScenario(node->scenario->name);
            result->reload_scenarios = true;
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
};

}  // namespace

std::unique_ptr<ScenarioBrowserComponent> CreateScenarioBrowserComponent(Application* app) {
  return std::make_unique<ScenarioBrowserComponentImpl>(app);
}

}  // namespace aim

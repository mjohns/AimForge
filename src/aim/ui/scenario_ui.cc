#include "scenario_ui.h"

#include <imgui.h>

#include <algorithm>

#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"

namespace aim {
namespace {

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
      app_->scenario_manager()->LoadScenariosFromDisk();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    auto search_words = GetSearchWords(search_text_);
    DrawScenarioNodes(app_->scenario_manager()->scenario_nodes(), search_words, result);
  }

 private:
  void DrawScenarioNodes(const std::vector<std::unique_ptr<ScenarioNode>>& nodes,
                         const std::vector<std::string>& search_words,
                         ScenarioBrowserResult* result) {
    PlaylistRun* current_playlist_run = app_->playlist_manager()->GetCurrentRun();
    ImGui::LoopId loop_id;
    for (auto& node : nodes) {
      auto id = loop_id.Get();
      if (node->scenario.has_value() && StringMatchesSearch(node->scenario->name, search_words)) {
        if (ImGui::Button(node->scenario->def.scenario_id().c_str())) {
          result->scenario_to_start = node->name;
        }
        if (current_playlist_run != nullptr) {
          if (ImGui::BeginPopupContextItem("scenario_item_menu")) {
            std::string playlist_name = current_playlist_run->playlist.name;
            std::string add_text = std::format("Add to \"{}\"", playlist_name);
            if (ImGui::Selectable(add_text.c_str())) {
              app_->playlist_manager()->AddScenarioToPlaylist(playlist_name, node->name);
            }
            ImGui::EndPopup();
          }
          ImGui::OpenPopupOnItemClick("scenario_item_menu", ImGuiPopupFlags_MouseButtonRight);
        }
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

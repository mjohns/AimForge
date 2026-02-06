#include "select_level_variation_dialog.h"

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "aim/common/search.h"
#include "aim/editor/scenario_editor_screen.h"
#include "imgui.h"

namespace aim {

bool SelectLevelVariationDialog::Draw(std::optional<float>* selected_level) {
  bool selected = false;
  ImGui::IdGuard cid("SelectLevelVariationDialog");
  ImGui::SetNextWindowPos(
      ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(id_.c_str(),
                             &draw_popup_,
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
    float char_x = ImGui::GetDefaultCharSizeX();
    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Scenario level")
                          .set_is_optional()
                          .set_step(1, 2)
                          .set_default(1)
                          .set_width(char_x * 10),
                      CreateOptionalFloatField(&input_level_));
    ImGui::SameLine();
    if (ImGui::Button("Select")) {
      *selected_level = input_level_;
      selected = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      draw_popup_ = false;
      ImGui::CloseCurrentPopup();
    }

    if (selected) {
      draw_popup_ = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (do_open_) {
    do_open_ = false;
    ImGui::OpenPopup(id_.c_str());
    draw_popup_ = true;
  }

  return selected;
}

}  // namespace aim

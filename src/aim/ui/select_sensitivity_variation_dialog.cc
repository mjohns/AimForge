#include "select_sensitivity_variation_dialog.h"

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "aim/common/search.h"
#include "aim/ui/scenario_editor_screen.h"
#include "google/protobuf/util/message_differencer.h"
#include "imgui.h"

namespace aim {

bool SelectSensitivityVariationDialog::Draw(std::optional<float>* selected_cm_per_360) {
  bool selected = false;
  ImGui::IdGuard cid("SelectSensitivityVariationDialog");
  ImGui::SetNextWindowPos(
      ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(id_.c_str(),
                             &draw_popup_,
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
    ImVec2 char_size = ImGui::CalcTextSize("A");
    for (int i = 10; i <= 70; i += 10) {
      std::string sens1 = std::format("{}cm", i);
      std::string sens2 = std::format("{}cm", i + 5);
      if (ImGui::Button(sens1)) {
        *selected_cm_per_360 = (float)i;
        selected = true;
      }
      ImGui::SameLine();
      if (ImGui::Button(sens2)) {
        *selected_cm_per_360 = i + 5.0f;
        selected = true;
      }
    }

    ImGui::Separator();

    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("cm/360")
                          .set_step(1, 5)
                          .set_precision(1)
                          .set_width(char_size.x * 10)
                          .set_min(1),
                      CreateFloatField(&input_cm_per_360_));
    ImGui::SameLine();
    if (ImGui::Button("Select")) {
      *selected_cm_per_360 = input_cm_per_360_;
      selected = true;
    }

    ImGui::Separator();

    if (ImGui::Button("Clear")) {
      *selected_cm_per_360 = {};
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

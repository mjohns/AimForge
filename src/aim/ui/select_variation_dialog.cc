#include "select_variation_dialog.h"

#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "imgui.h"

namespace aim {

bool SelectVariationDialog::Draw(std::string* updated_name) {
  ImGui::IdGuard cid("SelectVariationDialog_" + id_);
  bool selected = false;
  bool is_scenario = !is_playlist_;
  if (popup_.Begin()) {
    ImGui::Spacing();
    ImGui::Text(name_info_.GetFullName());

    ImGui::Spacing();

    if (ImGui::Button("Select")) {
      selected = true;
      popup_.Close();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      popup_.Close();
    }

    ImGui::SpacedSeparator();

    float char_x = ImGui::GetDefaultCharSizeX();
    if (is_scenario || is_playlist_) {
      ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("Level")
                            .set_is_optional()
                            .set_step(1, 2)
                            .set_default(1)
                            .set_width(char_x * 10),
                        CreateOptionalFloatField(&name_info_.level));
      ImGui::SpacedSeparator();
    }

    // Sensitivity variation selection
    ImGui::Text("Sensitivity");
    ImGui::Indent();

    ImGui::InputFloat(ImGui::InputFloatParams::WithLabelAsId("cm/360")
                          .set_is_optional()
                          .set_step(1, 5)
                          .set_width(char_x * 10)
                          .set_default(45)
                          .set_min(1),
                      CreateOptionalFloatField(&name_info_.cm_per_360));

    ImGui::Spacing();

    for (int i = 10; i <= 70; i += 10) {
      std::string sens1 = std::format("{}cm", i);
      std::string sens2 = std::format("{}cm", i + 5);
      if (ImGui::Button(sens1)) {
        name_info_.cm_per_360 = (float)i;
        // selected = true;
      }
      ImGui::SameLine();
      if (ImGui::Button(sens2)) {
        name_info_.cm_per_360 = (float)i + 5.0f;
        // selected = true;
      }
    }

    ImGui::Unindent();
    popup_.End();
  }
  if (selected) {
    *updated_name = name_info_.GetFullName();
  }
  return selected;
}

}  // namespace aim

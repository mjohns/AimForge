#pragma once

#include <string>

#include "aim/proto/scenario.pb.h"

namespace ImGui {
class MultilineTextEntryDialog;
}  // namespace ImGui

namespace aim {

class Application;

void DrawScenarioTypeEditor(ScenarioDef& def,
                            Application* app,
                            std::string* error_message,
                            bool* editing_room,
                            ImGui::MultilineTextEntryDialog* description_dialog);

// Overflow information for the scenario type shown in the left panel. Should not
// be the primary details for the scenario type.
void DrawSecondaryScenarioTypeEditor(ScenarioDef& def);

}  // namespace aim

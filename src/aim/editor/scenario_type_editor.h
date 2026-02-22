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

}  // namespace aim

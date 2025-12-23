#include "aim/common/mat_icons.h"
#include "aim/common/util.h"
#include "aim/graphics/textures.h"
#include "aim/ui/settings_screen.h"
#include "aim/ui/ui_screen.h"
#include "home_screen.h"

namespace aim {

void DrawTopBar(UiScreen* screen) {
  ImGui::IdGuard cid("TopBar");
  Application& app = screen->app();

  int large_font_size = app.font_manager().large_font_size();
  ImGui::BeginChild("Header", ImVec2(0, large_font_size * 1.3));
  if (app.logo_texture().is_loaded()) {
    int size = app.font_manager().large_font_size();
    ImGui::Image(app.logo_texture().GetImTextureId(),
                 ImVec2(size + 6, size + 6),
                 ImVec2(0.0f, 0.0f),
                 ImVec2(1.0f, 1.0f));
    ImGui::SameLine();
  }
  {
    auto font = app.font_manager().UseLargeBold();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("AimForge");
  }

  auto font = app.font_manager().UseLarge();
  auto current_scenario = app.scenario_manager().GetCurrentScenario();
  if (current_scenario) {
    float available_height = ImGui::GetContentRegionAvail().y + ImGui::GetCursorPosY();
    float button_height = ImGui::GetFrameHeight();

    ImGui::SameLine();
    float y_off = (available_height - button_height) / 2.0f;
    if (y_off > 0) {
      // ImGui::Dummy(ImVec2(0, y_off));
      //  TODO: This vertical centering is not working.
      // ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_off);
    }
    ImGui::AlignTextToFramePadding();
    ImGui::Text("                ");
    ImGui::SameLine();

    if (ImGui::Button(std::format("{}", kIconPlayArrow))) {
      app.state().scenario_run_option = ScenarioRunOption::START_CURRENT;
      screen->ReturnHome();
    }
    {
      auto normal_font = app.font_manager().UseDefault();
      ImGui::HelpTooltip("Start new run");
    }

    ImGui::SameLine();
    if (ImGui::Button(std::format("{}", kIconArrowForward))) {
      app.state().scenario_run_option = ScenarioRunOption::PLAYLIST_NEXT;
      screen->ReturnHome();
    }
    {
      auto normal_font = app.font_manager().UseDefault();
      ImGui::HelpTooltip("Playlist next");
    }

    ImGui::SameLine();
    ImGui::Text(current_scenario->id());
  }

  ImGui::SameLine();

  ImVec2 char_size = ImGui::CalcTextSize("A");
  ImGui::SetCursorAtRight(char_size.x * 2);
  if (ImGui::Selectable(kIconSettings, false)) {
    std::string current_scenario_name = current_scenario ? current_scenario->id() : "";
    screen->PushNextScreen(CreateSettingsScreen(&app, current_scenario_name));
  }

  font.Pop();
  ImGui::EndChild();
}

}  // namespace aim

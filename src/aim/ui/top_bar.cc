#include "top_bar.h"

#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/core/scenario_manager.h"
#include "aim/graphics/textures.h"
#include "aim/ui/crosshair_editor_screen.h"
#include "aim/ui/play_time_screen.h"
#include "aim/ui/reaction_time_screen.h"
#include "aim/ui/settings_screen.h"
#include "aim/ui/theme_editor_screen.h"
#include "aim/ui/ui_screen.h"

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
    ImGui::Text("FpsAimForge");
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

    if (ImGui::Button(std::format("{}", icons::kPlayArrow))) {
      app.state().scenario_run_option = ScenarioRunOption::START_CURRENT;
      screen->ReturnHome();
    }
    {
      auto normal_font = app.font_manager().UseDefault();
      ImGui::HelpTooltip("Start new run");
    }

    ImGui::SameLine();
    ImGui::Text(current_scenario->name);

    /*
    ImGui::SameLine();
    if (ImGui::Button(std::format("{}", icons::kArrowForward))) {
      app.state().scenario_run_option = ScenarioRunOption::PLAYLIST_NEXT;
      screen->ReturnHome();
    }
    {
      auto normal_font = app.font_manager().UseDefault();
      ImGui::HelpTooltip("Playlist next");
    }
    */
  }

  ImGui::SameLine();

  const char* menu_id = "top_bar_menu";
  if (ImGui::BeginPopupContextItem(menu_id)) {
    auto normal_font = app.font_manager().UseDefault();
    if (ImGui::Selectable(std::format("{} Settings", icons::kSettings).c_str())) {
      std::string current_scenario_name = current_scenario ? current_scenario->name : "";
      screen->PushNextScreen(CreateSettingsScreen(&app, current_scenario_name));
    }
    if (ImGui::Selectable(std::format("{} Themes", icons::kPalette).c_str(), false)) {
      screen->PushNextScreen(CreateThemeEditorScreen(&app));
    }
    if (ImGui::Selectable(std::format("{} Crosshairs", icons::kMyLocation).c_str(), false)) {
      screen->PushNextScreen(CreateCrosshairEditorScreen(&app));
    }
    if (ImGui::Selectable(std::format("{} Play time", icons::kHourglassEmpty).c_str(), false)) {
      screen->PushNextScreen(CreatePlayTimeScreen(&app));
    }
    if (ImGui::Selectable(std::format("{} Reaction", icons::kTimer).c_str(), false)) {
      screen->PushNextScreen(CreateReactionTimeScreen(&app));
    }

    ImGui::SpacedSeparator();

    if (ImGui::Selectable(std::format("{} Restart", icons::kRefresh).c_str())) {
      app.RequestRestart();
    }

    if (ImGui::Selectable(std::format("{} Exit", icons::kLogout).c_str())) {
      // Show a screen to confirm?
      app.RequestExit();
    }

    normal_font.Pop();
    ImGui::EndPopup();
  }
  float char_x = ImGui::GetDefaultCharSizeX();
  ImGui::SetCursorAtRight(char_x * 2);
  if (ImGui::SelectableButton(icons::kMoreVert)) {
    ImGui::OpenPopup(menu_id);
  }

  font.Pop();
  ImGui::EndChild();
}

}  // namespace aim

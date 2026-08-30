#include "top_bar.h"

#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/core/scenario_manager.h"
#include "aim/graphics/textures.h"
#include "aim/ui/crosshair_editor_screen.h"
#include "aim/ui/play_time_screen.h"
#include "aim/ui/reaction_time_screen.h"
#include "aim/ui/select_variation_dialog.h"
#include "aim/ui/settings_screen.h"
#include "aim/ui/theme_editor_screen.h"
#include "aim/ui/ui_screen.h"
#include "imgui.h"

namespace aim {
namespace {

class TopBarImpl : public TopBar {
 public:
  void Draw() override {
    ImGui::IdGuard cid("TopBar");

    std::string selected_scenario_name;
    if (select_variation_dialog_.Draw(&selected_scenario_name)) {
      app_.scenario_manager().SetCurrentScenario(selected_scenario_name);
    }

    int large_font_size = app_.font_manager().large_font_size();
    ImGui::BeginChild("Header", ImVec2(0, large_font_size * 1.3));
    if (app_.logo_texture().is_loaded()) {
      int size = app_.font_manager().large_font_size();
      ImGui::Image(app_.logo_texture().GetImTextureId(),
                   ImVec2(size + 6, size + 6),
                   ImVec2(0.0f, 0.0f),
                   ImVec2(1.0f, 1.0f));
      ImGui::SameLine();
    }
    {
      auto font = app_.font_manager().UseLargeBold();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("FpsAimForge");
    }

    auto font = app_.font_manager().UseLarge();
    auto current_scenario = app_.scenario_manager().GetCurrentScenario();
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

      {
        // Draw the play button slightly smaller and circular.
        //
        // Alternatively we could just change the font size and still do the cursor.y
        // adjustments.
        ImVec2 cursor = ImGui::GetCursorPos();
        float frame_height = ImGui::GetFrameHeight();
        float play_height = frame_height * 0.9;
        float height_diff = frame_height - play_height;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, play_height / 2.8f);
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(play_height * 0.05f, 0.0f));
        ImGui::SetCursorPosY(cursor.y + height_diff / 2.0);
        if (ImGui::Button(std::format("{}", icons::kPlayArrow), ImVec2(play_height, play_height))) {
          app_.state().scenario_run_option = ScenarioRunOption::START_CURRENT;
          app_.GetCurrentScreen()->ReturnHome();
        }
        ImGui::PopStyleVar(3);
        ImGui::SetCursorPosY(cursor.y);
      }

      ImGui::SameLine();
      ImGui::Text(current_scenario->name + "  ");

      ImGui::SameLine();
      if (ImGui::IconButton(icons::kTune)) {
        select_variation_dialog_.NotifyOpen(current_scenario->name);
      }
      {
        auto normal_font = app_.font_manager().UseDefault();
        ImGui::HelpTooltip("Select scenario variation");
      }

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
    if (ImGui::BeginPopup(menu_id)) {
      auto normal_font = app_.font_manager().UseDefault();
      if (ImGui::Selectable(std::format("{} Settings", icons::kSettings).c_str())) {
        std::string current_scenario_name = current_scenario ? current_scenario->name : "";
        app_.PushNextScreen(CreateSettingsScreen(current_scenario_name));
      }
      if (ImGui::Selectable(std::format("{} Themes", icons::kPalette).c_str(), false)) {
        app_.PushNextScreen(CreateThemeEditorScreen());
      }
      if (ImGui::Selectable(std::format("{} Crosshairs", icons::kMyLocation).c_str(), false)) {
        app_.PushNextScreen(CreateCrosshairEditorScreen());
      }
      if (ImGui::Selectable(std::format("{} Play time", icons::kHourglassEmpty).c_str(), false)) {
        app_.PushNextScreen(CreatePlayTimeScreen());
      }
      if (ImGui::Selectable(std::format("{} Reaction", icons::kTimer).c_str(), false)) {
        app_.PushNextScreen(CreateReactionTimeScreen());
      }

      ImGui::SpacedSeparator();

      if (ImGui::Selectable(std::format("{} Restart", icons::kRefresh).c_str())) {
        app_.RequestRestart();
      }

      if (ImGui::Selectable(std::format("{} Exit", icons::kLogout).c_str())) {
        // Show a screen to confirm?
        app_.RequestExit();
      }

      normal_font.Pop();
      ImGui::EndPopup();
    }
    ImGui::SetCursorAtRight(ImGui::GetMenuButtonWidth() * 1.2);
    if (ImGui::MenuButton()) {
      ImGui::OpenPopup(menu_id);
    }

    font.Pop();
    ImGui::EndChild();
  }

 private:
  Application& app_ = GetUiApp();
  SelectVariationDialog select_variation_dialog_ =
      SelectVariationDialog::ForScenarios("TopBarSelectScenarioVariation");
};
}  // namespace

std::unique_ptr<TopBar> CreateTopBar() {
  return std::make_unique<TopBarImpl>();
}

}  // namespace aim

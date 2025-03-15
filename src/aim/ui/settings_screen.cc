#include "settings_screen.h"

#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <optional>

#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"

namespace aim {
namespace {

class SettingsScreen : public UiScreen {
 public:
  explicit SettingsScreen(Application* app, const std::string& scenario_id)
      : UiScreen(app),
        settings_updater_(app->settings_manager(), app->history_db()),
        mgr_(app->settings_manager()),
        scenario_id_(scenario_id) {
    theme_names_ = app->settings_manager()->ListThemes();
    // Always try to save when exiting settings screen.
    app->settings_manager()->MarkDirty();

    Settings settings = mgr_->GetCurrentSettings();
    for (auto& c : settings.saved_crosshairs()) {
      crosshair_names_.push_back(c.name());
    }
  }

 protected:
  void DrawScreen() override {
    const ScreenInfo& screen = app_->screen_info();
    ImGui::Columns(2, "SettingsColumns", false);

    float left_width = screen.width * 0.15;
    ImGui::SetColumnWidth(0, left_width);
    ImGui::NextColumn();

    {
      auto font = app_->font_manager()->UseLarge();
      ImGui::Text("Settings");
      ImGui::Spacing();
      ImGui::Spacing();
    }

    ImVec2 char_size = ImGui::CalcTextSize("A");

    ImGui::Text("DPI");
    ImGui::SameLine();
    ImGui::PushItemWidth(char_size.x * 5);
    ImGui::InputText("##DPI", &settings_updater_.dpi, ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopItemWidth();

    ImGui::Text("CM/360");
    ImGui::SameLine();
    ImGui::PushItemWidth(char_size.x * 4);
    ImGui::InputText("##CM/360", &settings_updater_.cm_per_360, ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopItemWidth();

    ImGui::Text("Metronome BPM");
    ImGui::SameLine();
    ImGui::PushItemWidth(char_size.x * 4);
    ImGui::InputText(
        "##Metronome_BPM", &settings_updater_.metronome_bpm, ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopItemWidth();

    ImGui::Text("Theme");
    ImGui::SameLine();
    ImGuiComboFlags combo_flags = 0;
    ImGui::PushItemWidth(char_size.x * 20);
    if (ImGui::BeginCombo("##theme_combo", settings_updater_.theme_name.c_str(), combo_flags)) {
      for (int i = 0; i < theme_names_.size(); ++i) {
        auto& theme_name = theme_names_[i];
        bool is_selected = theme_name == settings_updater_.theme_name;
        if (ImGui::Selectable(std::format("{}##{}theme_name", theme_name, i).c_str(),
                              is_selected)) {
          settings_updater_.theme_name = theme_name;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    ImGui::PopItemWidth();
    ImGui::Text("Crosshair");
    ImGui::SameLine();
    ImGui::PushItemWidth(char_size.x * 15);
    if (ImGui::BeginCombo(
            "##crosshair_combo", settings_updater_.crosshair_name.c_str(), combo_flags)) {
      for (int i = 0; i < crosshair_names_.size(); ++i) {
        auto& crosshair_name = crosshair_names_[i];
        bool is_selected = crosshair_name == settings_updater_.crosshair_name;
        if (ImGui::Selectable(std::format("{}##{}crosshair_name", crosshair_name, i).c_str(),
                              is_selected)) {
          settings_updater_.crosshair_name = crosshair_name;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::Text("Crosshair Size");
    ImGui::SameLine();
    ImGui::PushItemWidth(char_size.x * 4);
    ImGui::InputText(
        "##CrosshairSize", &settings_updater_.crosshair_size, ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopItemWidth();

    {
      ImVec2 sz = ImVec2(0.0f, 0.0f);
      if (ImGui::Button("Save", sz)) {
        settings_updater_.SaveIfChangesMade(scenario_id_);
        Done();
      }
    }
  }

  std::optional<NavigationEvent> OnKeyUp(const SDL_Event& event, bool user_is_typing) override {
    SDL_Keycode keycode = event.key.key;
    if (!user_is_typing && keycode == SDLK_ESCAPE) {
      settings_updater_.SaveIfChangesMade(scenario_id_);
      return NavigationEvent::Done();
    }

    return {};
  }

 private:
  SettingsUpdater settings_updater_;
  SettingsManager* mgr_;
  std::vector<std::string> theme_names_;
  std::vector<std::string> crosshair_names_;
  const std::string scenario_id_;
};
}  // namespace

std::unique_ptr<UiScreen> CreateSettingsScreen(Application* app,
                                               const std::string& current_scenario_id) {
  return std::make_unique<SettingsScreen>(app, current_scenario_id);
}

}  // namespace aim

#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <optional>

#include "aim/common/util.h"
#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"
#include "aim/scenario/screens.h"
#include "aim/ui/ui_screen.h"

namespace aim {
namespace {

class QuickSettingsScreen : public UiScreen {
 public:
  explicit QuickSettingsScreen(const std::string& scenario_id,
                               Application* app,
                               QuickSettingsType type)
      : UiScreen(app),
        scenario_id_(scenario_id),
        mgr_(app->settings_manager()),
        updater_(app->settings_manager(), app->history_db()),
        type_(type) {
    theme_names_ = app->settings_manager()->ListThemes();
    Settings settings = mgr_->GetCurrentSettings();
    for (auto& c : settings.saved_crosshairs()) {
      crosshair_names_.push_back(c.name());
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      if (event.wheel.y != 0) {
        if (type_ == QuickSettingsType::DEFAULT) {
          float cm_per_360_val = ParseFloat(updater_.cm_per_360);
          updater_.cm_per_360 = std::format("{}", cm_per_360_val + event.wheel.y);
        }
        if (type_ == QuickSettingsType::METRONOME) {
          float bpm = ParseFloat(updater_.metronome_bpm);
          updater_.metronome_bpm = std::format("{}", bpm + event.wheel.y);
        }
      }
    }
  }

  std::optional<NavigationEvent> OnKeyUp(const SDL_Event& event, bool user_is_typing) override {
    SDL_Keycode keycode = event.key.key;
    if (keycode == SDLK_S || keycode == SDLK_B || keycode == SDLK_C) {
      updater_.SaveIfChangesMade(scenario_id_);
      return NavigationEvent::Done();
    }
    return {};
  }

 protected:
  void DrawScreen() override {
    const ScreenInfo& screen = app_->screen_info();
    ImGui::Columns(3, "SettingsColumns", false);

    float width = screen.width * 0.5;
    float x_start = (screen.width - width) * 0.5;
    ImGui::SetColumnWidth(0, x_start);
    ImGui::SetColumnWidth(1, width);

    ImGui::NextColumn();
    ImVec2 char_size = ImGui::CalcTextSize("A");

    float center_gap = 10;

    // ImGui::SetCursorPos(ImVec2(x_start, y_start));
    ImGui::SetCursorPosY(screen.height * 0.3);

    ImVec2 button_sz = ImVec2((width - center_gap) / 2.0, 40);

    if (type_ == QuickSettingsType::DEFAULT) {
      for (int i = 20; i <= 70; i += 10) {
        std::string sens1 = std::format("{}", i);
        std::string sens2 = std::format("{}", i + 5);
        if (ImGui::Button(sens1.c_str(), button_sz)) {
          updater_.cm_per_360 = sens1;
        }
        ImGui::SameLine();
        // ImGui::SetCursorPos(ImVec2(x_start, y_start));
        if (ImGui::Button(sens2.c_str(), button_sz)) {
          updater_.cm_per_360 = sens2;
        }
      }

      // ImGui::SetCursorPos(ImVec2(x_start, y_start));

      ImGui::Spacing();
      ImGui::PushItemWidth(button_sz.x);
      ImGui::InputText("##CM_PER_360", &updater_.cm_per_360, ImGuiInputTextFlags_CharsDecimal);
      ImGui::PopItemWidth();

      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();

      ImGuiComboFlags combo_flags = 0;
      ImGui::Text("Theme");
      ImGui::SameLine();
      ImGui::PushItemWidth(char_size.x * 20);
      if (ImGui::BeginCombo("##theme_combo", updater_.theme_name.c_str(), combo_flags)) {
        for (int i = 0; i < theme_names_.size(); ++i) {
          auto& theme_name = theme_names_[i];
          bool is_selected = theme_name == updater_.theme_name;
          if (ImGui::Selectable(std::format("{}##{}theme_name", theme_name, i).c_str(),
                                is_selected)) {
            updater_.theme_name = theme_name;
          }
          if (is_selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::Text("Crosshair");
      ImGui::SameLine();
      ImGui::PushItemWidth(char_size.x * 15);
      if (ImGui::BeginCombo("##crosshair_combo", updater_.crosshair_name.c_str(), combo_flags)) {
        for (int i = 0; i < crosshair_names_.size(); ++i) {
          auto& crosshair_name = crosshair_names_[i];
          bool is_selected = crosshair_name == updater_.crosshair_name;
          if (ImGui::Selectable(std::format("{}##{}crosshair_name", crosshair_name, i).c_str(),
                                is_selected)) {
            updater_.crosshair_name = crosshair_name;
          }
          if (is_selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    }

    if (type_ == QuickSettingsType::METRONOME) {
      for (int i = 100; i <= 160; i += 10) {
        std::string bpm1 = std::format("{}", i);
        std::string bpm2 = std::format("{}", i + 5);
        if (ImGui::Button(bpm1.c_str(), button_sz)) {
          updater_.metronome_bpm = bpm1;
        }
        ImGui::SameLine();
        // ImGui::SetCursorPos(ImVec2(x_start, y_start));
        if (ImGui::Button(bpm2.c_str(), button_sz)) {
          updater_.metronome_bpm = bpm2;
        }
      }

      // ImGui::SetCursorPos(ImVec2(x_start, y_start));

      ImGui::Spacing();
      ImGui::PushItemWidth(button_sz.x);
      if (ImGui::Button("0", button_sz)) {
        updater_.metronome_bpm = "0";
      }
      ImGui::SameLine();
      ImGui::InputText(
          "##METRONOME_BPM", &updater_.metronome_bpm, ImGuiInputTextFlags_CharsDecimal);
    }
  }

 private:
  std::string scenario_id_;
  SettingsManager* mgr_ = nullptr;
  SettingsUpdater updater_;
  QuickSettingsType type_;
  std::vector<std::string> theme_names_;
  std::vector<std::string> crosshair_names_;
};

}  // namespace

std::unique_ptr<UiScreen> CreateQuickSettingsScreen(const std::string& scenario_id,
                                                    QuickSettingsType type,
                                                    Application* app) {
  return std::make_unique<QuickSettingsScreen>(scenario_id, app, type);
}

}  // namespace aim

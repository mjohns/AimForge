#include "quick_settings_screen.h"

#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <optional>

#include "aim/common/util.h"
#include "aim/core/settings_manager.h"
#include "aim/ui/settings_screen.h"
#include "aim/ui/ui_screen.h"

namespace aim {
namespace {

class QuickSettingsScreen : public UiScreen {
 public:
  explicit QuickSettingsScreen(const std::string& scenario_id,
                               Application& app,
                               QuickSettingsType type,
                               const std::string& release_key)
      : UiScreen(app),
        scenario_id_(scenario_id),
        updater_(app.settings_manager().CreateUpdater()),
        type_(type),
        release_key_(release_key) {
    theme_names_ = app.settings_manager().ListThemes();
    crosshair_names_ = app_.settings_manager().ListCrosshairNames();
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      if (event.wheel.y != 0) {
        if (type_ == QuickSettingsType::DEFAULT) {
          updater_.settings.set_cm_per_360(updater_.settings.cm_per_360() + event.wheel.y);
        }
        if (type_ == QuickSettingsType::METRONOME) {
          updater_.settings.set_metronome_bpm(updater_.settings.metronome_bpm() + event.wheel.y);
        }
      }
    }
    if (IsMappableKeyUpEvent(event) && KeyNameMatchesEvent(event, release_key_)) {
      updater_.SaveIfChangesMade(scenario_id_);
      PopSelf();
    }
  }

 protected:
  void OnAttachUi() override {
    if (went_to_settings_) {
      // Returned from full settings so just exit quick settings too.
      PopSelf();
    }
  }

  void DrawScreen() override {
    if (app_.BeginFullscreenWindow()) {
      DrawScreenInternal();
    }
    ImGui::End();
  }

  void DrawScreenInternal() {
    const ScreenInfo& screen = app_.screen_info();
    if (ImGui::Button(std::format("{} Settings", kIconSettings))) {
      PushNextScreen(CreateSettingsScreen(&app_, scenario_id_));
      went_to_settings_ = true;
    }
    ImGui::Columns(3, "SettingsColumns", false);

    float width = screen.width * 0.5;
    float x_start = (screen.width - width) * 0.5;
    ImGui::SetColumnWidth(0, x_start);
    ImGui::SetColumnWidth(1, width);

    ImGui::NextColumn();
    ImVec2 char_size = ImGui::CalcTextSize("A");

    float center_gap = 10;

    ImGui::SetCursorPosY(screen.height * 0.3);

    ImVec2 button_sz = ImVec2((width - center_gap) / 2.0, 40);

    if (type_ == QuickSettingsType::DEFAULT) {
      for (int i = 20; i <= 70; i += 10) {
        std::string sens1 = std::format("{}", i);
        std::string sens2 = std::format("{}", i + 5);
        if (ImGui::Button(sens1.c_str(), button_sz)) {
          updater_.settings.set_cm_per_360(i);
        }
        ImGui::SameLine();
        if (ImGui::Button(sens2.c_str(), button_sz)) {
          updater_.settings.set_cm_per_360(i + 5);
        }
      }

      ImGui::Spacing();
      ImGui::InputJitteredFloat(ImGui::InputFloatParams("CmPer360")
                                    .set_label("cm/360")
                                    .set_step(1, 5)
                                    .set_width(char_size.x * 9)
                                    .set_min(1)
                                    .set_default(35),
                                PROTO_JITTERED_FIELD(Settings, &updater_.settings, cm_per_360));

      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Theme");
      ImGui::SameLine();
      ImGui::SimpleDropdown(
          "ThemeDropdown", updater_.settings.mutable_theme_name(), theme_names_, char_size.x * 20);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Crosshair");
      ImGui::SameLine();
      ImGui::SimpleDropdown("CrosshairDropdown",
                            updater_.settings.mutable_current_crosshair_name(),
                            crosshair_names_,
                            char_size.x * 15);

      ImGui::InputBool(ImGui::InputBoolParams("AutoHoldTracking").set_label("Auto hold tracking"),
                       PROTO_BOOL_FIELD(Settings, &updater_.settings, auto_hold_tracking));
      ImGui::InputBool(
          ImGui::InputBoolParams("ShowHealthBars").set_label("Show health bars"),
          PROTO_BOOL_FIELD(HealthBarSettings, updater_.settings.mutable_health_bar(), show));
    }

    if (type_ == QuickSettingsType::METRONOME) {
      for (int i = 100; i <= 160; i += 10) {
        std::string bpm1 = std::format("{}", i);
        std::string bpm2 = std::format("{}", i + 5);
        if (ImGui::Button(bpm1.c_str(), button_sz)) {
          updater_.settings.set_metronome_bpm(i);
        }
        ImGui::SameLine();
        if (ImGui::Button(bpm2.c_str(), button_sz)) {
          updater_.settings.set_metronome_bpm(i + 5);
        }
      }

      ImGui::Spacing();
      if (ImGui::Button("0", button_sz)) {
        updater_.settings.clear_metronome_bpm();
      }
      ImGui::SameLine();
      ImGui::InputFloat(ImGui::InputFloatParams("MetronomeBpm")
                            .set_label("BPM")
                            .set_min(0)
                            .set_zero_is_unset()
                            .set_step(1, 5)
                            .set_width(char_size.x * 9),
                        PROTO_FLOAT_FIELD(Settings, &updater_.settings, metronome_bpm));
    }
  }

 private:
  std::string scenario_id_;
  SettingsUpdater updater_;
  QuickSettingsType type_;
  std::vector<std::string> theme_names_;
  std::vector<std::string> crosshair_names_;
  std::string release_key_;
  bool went_to_settings_ = false;
};

}  // namespace

std::unique_ptr<UiScreen> CreateQuickSettingsScreen(const std::string& scenario_id,
                                                    QuickSettingsType type,
                                                    const std::string& release_key,
                                                    Application* app) {
  return std::make_unique<QuickSettingsScreen>(scenario_id, *app, type, release_key);
}

}  // namespace aim

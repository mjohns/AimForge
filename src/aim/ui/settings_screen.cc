#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <optional>

#include "aim/common/util.h"
#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"
#include "aim/ui/ui_screen.h"

namespace aim {
namespace {

class SettingsScreen : public UiScreen {
 public:
  explicit SettingsScreen(Application* app) : UiScreen(app), updater_(app->settings_manager()) {}

  std::optional<NavigationEvent> OnKeyDown(const SDL_Event& event, bool user_is_typing) override {
    SDL_Keycode keycode = event.key.key;
    if (keycode == SDLK_ESCAPE) {
      updater_.SaveIfChangesMade();
      return NavigationEvent::Done();
    }
    if (!user_is_typing) {
      if (keycode == SDLK_H) {
        return NavigationEvent::GoHome();
      }
      if (keycode == SDLK_R) {
        return NavigationEvent::RestartLastScenario();
      }
    }
    return {};
  }

 protected:
  void DrawScreen() override {
    const ScreenInfo& screen = app_->screen_info();
    float width = screen.width * 0.5;
    float height = screen.height * 0.9;

    float x_start = (screen.width - width) * 0.5;
    float y_start = (screen.height - height) * 0.5;

    // ImGui::SetCursorPos(ImVec2(x_start, y_start));

    ImGui::Spacing();
    ImGui::Indent(x_start);

    ImGui::Columns(2, "SettingsColumns", false);  // 2 columns, no borders

    ImGui::Text("CM/360");

    ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText("##CM_PER_360", &updater_.cm_per_360, ImGuiInputTextFlags_CharsDecimal);

    ImGui::NextColumn();
    ImGui::Text("Theme");

    ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText("##THEME_NAME", &updater_.theme_name);

    ImGui::NextColumn();
    ImGui::Text("Metronome BPM");

    ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText("##METRONOME_BPM", &updater_.metronome_bpm, ImGuiInputTextFlags_CharsDecimal);
  }

 private:
  SettingsUpdater updater_;
};

enum class QuickSettingsType { DEFAULT, METRONOME, CROSSHAIR };

class QuickSettingsScreen : public UiScreen {
 public:
  explicit QuickSettingsScreen(Application* app, QuickSettingsType type)
      : UiScreen(app),
        mgr_(app->settings_manager()),
        updater_(app->settings_manager()),
        type_(type) {}

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
        if (type_ == QuickSettingsType::CROSSHAIR) {
          float size = ParseFloat(updater_.crosshair_size);
          updater_.crosshair_size = std::format("{}", size + event.wheel.y);
        }
      }
    }
  }

  std::optional<NavigationEvent> OnKeyUp(const SDL_Event& event, bool user_is_typing) override {
    SDL_Keycode keycode = event.key.key;
    if (keycode == SDLK_S || keycode == SDLK_B || keycode == SDLK_C) {
      updater_.SaveIfChangesMade();
      return NavigationEvent::Done();
    }
    return {};
  }

 protected:
  void DrawScreen() override {
    const ScreenInfo& screen = app_->screen_info();
    float width = screen.width * 0.5;
    float center_gap = 10;

    float x_start = (screen.width - width) * 0.5;

    // ImGui::SetCursorPos(ImVec2(x_start, y_start));
    ImGui::SetCursorPos(ImVec2(0, screen.height * 0.3));

    ImVec2 button_sz = ImVec2((width - center_gap) / 2.0, 40);
    ImGui::Indent(x_start);

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

      if (ImGui::Button("static_default", button_sz)) {
        updater_.theme_name = "static_default";
      }
      ImGui::SameLine();
      if (ImGui::Button("concrete_moon", button_sz)) {
        updater_.theme_name = "concrete_moon";
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

    if (type_ == QuickSettingsType::CROSSHAIR) {
      ImGui::InputText(
          "##CROSSHAIR_SIZE", &updater_.crosshair_size, ImGuiInputTextFlags_CharsDecimal);
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      Crosshair crosshair = mgr_->GetCurrentSettings().crosshair();
      float size = ParseFloat(updater_.crosshair_size);
      crosshair.set_size(size);
      DrawCrosshair(crosshair, mgr_->GetCurrentTheme(), screen, draw_list);
    }
  }

 private:
  SettingsManager* mgr_ = nullptr;
  SettingsUpdater updater_;
  QuickSettingsType type_;
};

}  // namespace

std::unique_ptr<UiScreen> CreateSettingsScreen(Application* app, SettingsScreenType type) {
  switch (type) {
    case SettingsScreenType::QUICK:
      return std::make_unique<QuickSettingsScreen>(app, QuickSettingsType::DEFAULT);
    case SettingsScreenType::QUICK_METRONOME:
      return std::make_unique<QuickSettingsScreen>(app, QuickSettingsType::METRONOME);
    case SettingsScreenType::QUICK_CROSSHAIR:
      return std::make_unique<QuickSettingsScreen>(app, QuickSettingsType::CROSSHAIR);
  };
  return std::make_unique<SettingsScreen>(app);
}

}  // namespace aim

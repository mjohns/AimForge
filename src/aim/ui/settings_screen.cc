#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <optional>

#include "aim/common/util.h"
#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"
#include "aim/ui/ui_screen.h"

namespace aim {
namespace {
class SettingsScreen : public UiScreen {
 public:
  explicit SettingsScreen(Application* app) : UiScreen(app) {
    mgr_ = app->settings_manager();
    current_settings_ = mgr_->GetMutableCurrentSettings();
    if (current_settings_ != nullptr) {
      cm_per_360_ = MaybeIntToString(current_settings_->cm_per_360());
      theme_name_ = current_settings_->theme_name();
    }
  }

  std::optional<NavigationEvent> OnKeyDown(const SDL_Event& event, bool user_is_typing) override {
    SDL_Keycode keycode = event.key.key;
    if (keycode == SDLK_ESCAPE) {
      float new_cm_per_360 = ParseFloat(cm_per_360_);
      if (new_cm_per_360 > 0) {
        current_settings_->set_cm_per_360(new_cm_per_360);
        mgr_->MarkDirty();
      }
      if (current_settings_->theme_name() != theme_name_) {
        current_settings_->set_theme_name(theme_name_);
        mgr_->MarkDirty();
      }
      mgr_->MaybeFlushToDisk();
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
    ImGui::InputText("##CM_PER_360", &cm_per_360_, ImGuiInputTextFlags_CharsDecimal);

    ImGui::NextColumn();
    ImGui::Text("Theme");

    ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText("##THEME_NAME", &theme_name_);
  }

 private:
  std::string cm_per_360_;
  std::string theme_name_;
  SettingsManager* mgr_ = nullptr;
  Settings* current_settings_ = nullptr;
};

class QuickSettingsScreen : public UiScreen {
 public:
  explicit QuickSettingsScreen(Application* app) : UiScreen(app) {
    mgr_ = app->settings_manager();
    current_settings_ = mgr_->GetMutableCurrentSettings();
    if (current_settings_ != nullptr) {
      cm_per_360_ = MaybeIntToString(current_settings_->cm_per_360());
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      if (event.wheel.y != 0) {
        float cm_per_360_val = ParseFloat(cm_per_360_);
        cm_per_360_ = std::format("{}", cm_per_360_val + event.wheel.y);
      }
    }
  }

  std::optional<NavigationEvent> OnKeyUp(const SDL_Event& event, bool user_is_typing) override {
    SDL_Keycode keycode = event.key.key;
    if (keycode == SDLK_S) {
      float new_cm_per_360 = ParseFloat(cm_per_360_);
      if (new_cm_per_360 > 0 && current_settings_->cm_per_360() != new_cm_per_360) {
        current_settings_->set_cm_per_360(new_cm_per_360);
        mgr_->MarkDirty();
      }
      mgr_->MaybeFlushToDisk();
      return NavigationEvent::Done();
    }
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
    for (int i = 20; i <= 70; i += 10) {
      std::string sens1 = std::format("{}", i);
      std::string sens2 = std::format("{}", i + 5);
      if (ImGui::Button(sens1.c_str(), button_sz)) {
        cm_per_360_ = sens1;
      }
      ImGui::SameLine();
      // ImGui::SetCursorPos(ImVec2(x_start, y_start));
      if (ImGui::Button(sens2.c_str(), button_sz)) {
        cm_per_360_ = sens2;
      }
    }

    // ImGui::SetCursorPos(ImVec2(x_start, y_start));

    ImGui::Spacing();
    ImGui::PushItemWidth(button_sz.x);
    ImGui::InputText("##CM_PER_360", &cm_per_360_, ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopItemWidth();
  }

 private:
  std::string cm_per_360_;
  std::string theme_name_;
  SettingsManager* mgr_ = nullptr;
  Settings* current_settings_ = nullptr;
};

}  // namespace

std::unique_ptr<UiScreen> CreateSettingsScreen(Application* app) {
  return std::make_unique<SettingsScreen>(app);
}

std::unique_ptr<UiScreen> CreateQuickSettingsScreen(Application* app) {
  return std::make_unique<QuickSettingsScreen>(app);
}

}  // namespace aim

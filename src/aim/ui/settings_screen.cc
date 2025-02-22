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
  explicit SettingsScreen(Application* app) : UiScreen(app) {
    mgr_ = app->settings_manager();
    current_settings_ = mgr_->GetMutableCurrentSettings();
    if (current_settings_ != nullptr) {
      cm_per_360_ = MaybeIntToString(current_settings_->cm_per_360());
      theme_name_ = current_settings_->theme_name();
      metronome_bpm_ = MaybeIntToString(current_settings_->metronome_bpm());
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
      float new_metronome_bpm = ParseFloat(metronome_bpm_);
      if (new_metronome_bpm >= 0 && current_settings_->metronome_bpm() != new_metronome_bpm) {
        current_settings_->set_metronome_bpm(new_metronome_bpm);
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

    ImGui::NextColumn();
    ImGui::Text("Metronome BPM");

    ImGui::SetCursorPosX(screen.width * 0.5);
    ImGui::NextColumn();
    ImGui::InputText("##METRONOME_BPM", &metronome_bpm_, ImGuiInputTextFlags_CharsDecimal);
  }

 private:
  std::string cm_per_360_;
  std::string theme_name_;
  std::string metronome_bpm_;
  std::string crosshair_size_;
  SettingsManager* mgr_ = nullptr;
  Settings* current_settings_ = nullptr;
};

enum class QuickSettingsType { DEFAULT, METRONOME, CROSSHAIR };

class QuickSettingsScreen : public UiScreen {
 public:
  explicit QuickSettingsScreen(Application* app, QuickSettingsType type)
      : UiScreen(app), type_(type) {
    mgr_ = app->settings_manager();
    current_settings_ = mgr_->GetMutableCurrentSettings();
    if (current_settings_ != nullptr) {
      cm_per_360_ = MaybeIntToString(current_settings_->cm_per_360());
      metronome_bpm_ = MaybeIntToString(current_settings_->metronome_bpm());
      theme_name_ = current_settings_->theme_name();
      crosshair_size_ = MaybeIntToString(current_settings_->crosshair().size());
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      if (event.wheel.y != 0) {
        if (type_ == QuickSettingsType::DEFAULT) {
          float cm_per_360_val = ParseFloat(cm_per_360_);
          cm_per_360_ = std::format("{}", cm_per_360_val + event.wheel.y);
        }
        if (type_ == QuickSettingsType::METRONOME) {
          float bpm = ParseFloat(metronome_bpm_);
          metronome_bpm_ = std::format("{}", bpm + event.wheel.y);
        }
        if (type_ == QuickSettingsType::CROSSHAIR) {
          float size = ParseFloat(crosshair_size_);
          crosshair_size_ = std::format("{}", size + event.wheel.y);
        }
      }
    }
  }

  std::optional<NavigationEvent> OnKeyUp(const SDL_Event& event, bool user_is_typing) override {
    SDL_Keycode keycode = event.key.key;
    if (keycode == SDLK_S || keycode == SDLK_B || keycode == SDLK_C) {
      float new_cm_per_360 = ParseFloat(cm_per_360_);
      if (new_cm_per_360 > 0 && current_settings_->cm_per_360() != new_cm_per_360) {
        current_settings_->set_cm_per_360(new_cm_per_360);
        mgr_->MarkDirty();
      }
      float new_metronome_bpm = ParseFloat(metronome_bpm_);
      if (new_metronome_bpm >= 0 && current_settings_->metronome_bpm() != new_metronome_bpm) {
        current_settings_->set_metronome_bpm(new_metronome_bpm);
        mgr_->MarkDirty();
      }
      float new_crosshair_size = ParseFloat(crosshair_size_);
      if (new_crosshair_size >= 0 && current_settings_->crosshair().size() != new_crosshair_size) {
        current_settings_->mutable_crosshair()->set_size(new_crosshair_size);
        mgr_->MarkDirty();
      }
      if (theme_name_ != current_settings_->theme_name()) {
        current_settings_->set_theme_name(theme_name_);
        mgr_->MarkDirty();
      }
      mgr_->MaybeFlushToDisk();
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

      ImGui::Spacing();
      ImGui::Spacing();
      ImGui::Spacing();

      if (ImGui::Button("Default Theme", button_sz)) {
        theme_name_ = "default";
      }
      ImGui::SameLine();
      if (ImGui::Button("Texture Theme", button_sz)) {
        theme_name_ = "dark_herringbone";
      }
    }

    if (type_ == QuickSettingsType::METRONOME) {
      for (int i = 100; i <= 160; i += 10) {
        std::string bpm1 = std::format("{}", i);
        std::string bpm2 = std::format("{}", i + 5);
        if (ImGui::Button(bpm1.c_str(), button_sz)) {
          metronome_bpm_ = bpm1;
        }
        ImGui::SameLine();
        // ImGui::SetCursorPos(ImVec2(x_start, y_start));
        if (ImGui::Button(bpm2.c_str(), button_sz)) {
          metronome_bpm_ = bpm2;
        }
      }

      // ImGui::SetCursorPos(ImVec2(x_start, y_start));

      ImGui::Spacing();
      ImGui::PushItemWidth(button_sz.x);
      if (ImGui::Button("0", button_sz)) {
        metronome_bpm_ = "0";
      }
      ImGui::SameLine();
      ImGui::InputText("##METRONOME_BPM", &metronome_bpm_, ImGuiInputTextFlags_CharsDecimal);
    }

    if (type_ == QuickSettingsType::CROSSHAIR) {
      ImGui::InputText("##CROSSHAIR_SIZE", &crosshair_size_, ImGuiInputTextFlags_CharsDecimal);
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      Crosshair crosshair = mgr_->GetCurrentSettings().crosshair();
      float size = ParseFloat(crosshair_size_);
      crosshair.set_size(size);
      DrawCrosshair(crosshair, mgr_->GetCurrentTheme(), screen, draw_list);
    }
  }

 private:
  std::string cm_per_360_;
  std::string theme_name_;
  std::string metronome_bpm_;
  std::string crosshair_size_;
  SettingsManager* mgr_ = nullptr;
  Settings* current_settings_ = nullptr;
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

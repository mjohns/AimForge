#include "settings_screen.h"

#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"

namespace aim {
namespace {

struct KeybindItem {
  std::string label;
  KeyMapping* mapping;
  int is_capturing_index = 0;
};

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

    keybind_items_ = {
        {"Fire", settings_updater_.keybinds.mutable_fire()},
        {"Restart Scenario", settings_updater_.keybinds.mutable_restart_scenario()},
        {"Next Scenario", settings_updater_.keybinds.mutable_next_scenario()},
        {"Edit Scenario", settings_updater_.keybinds.mutable_edit_scenario()},
        {"Quick Settings", settings_updater_.keybinds.mutable_quick_settings()},
        {"Quick Metronome", settings_updater_.keybinds.mutable_quick_metronome()},
        {"Adjust Crosshair Size", settings_updater_.keybinds.mutable_adjust_crosshair_size()},
    };
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
    ImGui::SameLine();
    ImGui::Text("+/-");
    ImGui::SameLine();
    ImGui::PushItemWidth(char_size.x * 3);
    ImGui::InputText("##CM_PER_360_JITTER",
                     &settings_updater_.cm_per_360_jitter,
                     ImGuiInputTextFlags_CharsDecimal);
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

    ImGui::Text("Disable \"Click to Start\"");
    ImGui::SameLine();
    ImGui::Checkbox("##disable_click_to_start", &settings_updater_.disable_click_to_start);

    ImGui::Text("Auto Hold Tracking");
    ImGui::SameLine();
    ImGui::Checkbox("##auto_hold_tracking", &settings_updater_.auto_hold_tracking);

    for (KeybindItem& item : keybind_items_) {
      ImGui::Text(item.label);
      float entry_width = char_size.x * 10;
      ImGui::SameLine();
      KeyMappingEntry(&item, 1, entry_width);
      ImGui::SameLine();
      KeyMappingEntry(&item, 2, entry_width);
      ImGui::SameLine();
      KeyMappingEntry(&item, 3, entry_width);
      ImGui::SameLine();
      KeyMappingEntry(&item, 4, entry_width);
    }

    {
      ImVec2 sz = ImVec2(char_size.x * 14, 0.0f);
      if (ImGui::Button("Save", sz)) {
        settings_updater_.SaveIfChangesMade(scenario_id_);
        ScreenDone();
      }
    }
    {
      ImGui::SameLine();
      ImVec2 sz = ImVec2(0, 0.0f);
      if (ImGui::Button("Cancel", sz)) {
        ScreenDone();
      }
    }
  }

  void KeyMappingEntry(KeybindItem* item, int i, float width) {
    std::string value;
    if (item->is_capturing_index == i) {
      value = "...";
    } else if (i == 1) {
      value = item->mapping->mapping1();
    } else if (i == 2) {
      value = item->mapping->mapping2();
    } else if (i == 3) {
      value = item->mapping->mapping3();
    } else if (i == 4) {
      value = item->mapping->mapping4();
    }

    if (ImGui::Button(std::format("{}##key{}_{}", value, i, item->label).c_str(),
                      ImVec2(width, 0))) {
      for (KeybindItem& other_item : keybind_items_) {
        other_item.is_capturing_index = 0;
      }

      item->is_capturing_index = i;
      capture_key_fn_ = [item, i](const SDL_Event& event) {
        std::string key_value = SDL_GetKeyName(event.key.key);
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
          key_value = GetMouseButtonName(event.button.button);
        }
        if (i == 1) {
          item->mapping->set_mapping1(key_value);
        } else if (i == 2) {
          item->mapping->set_mapping2(key_value);
        } else if (i == 3) {
          item->mapping->set_mapping3(key_value);
        } else if (i == 4) {
          item->mapping->set_mapping4(key_value);
        }
        item->is_capturing_index = 0;
      };
    }
    ImGui::SameLine();
    if (ImGui::Button(std::format("x##clear_{}_{}", i, item->label).c_str(), ImVec2(0, 0))) {
      if (i == 1) {
        item->mapping->set_mapping1("");
      } else if (i == 2) {
        item->mapping->set_mapping2("");
      } else if (i == 3) {
        item->mapping->set_mapping3("");
      } else if (i == 4) {
        item->mapping->set_mapping4("");
      }
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    bool is_capturable =
        event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    if (capture_key_fn_ && is_capturable) {
      auto capture = capture_key_fn_;
      capture_key_fn_ = {};
      capture(event);
    }
  }

 private:
  SettingsUpdater settings_updater_;
  SettingsManager* mgr_;
  std::vector<std::string> theme_names_;
  std::vector<std::string> crosshair_names_;
  const std::string scenario_id_;

  std::function<void(const SDL_Event&)> capture_key_fn_;
  std::vector<KeybindItem> keybind_items_;
};
}  // namespace

std::unique_ptr<UiScreen> CreateSettingsScreen(Application* app,
                                               const std::string& current_scenario_id) {
  return std::make_unique<SettingsScreen>(app, current_scenario_id);
}

}  // namespace aim

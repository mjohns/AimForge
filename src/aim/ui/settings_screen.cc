#include "settings_screen.h"

#include <format>
#include <functional>
#include <optional>

#include "aim/common/field.h"
#include "aim/common/files.h"
#include "aim/common/imgui_ext.h"
#include "aim/core/settings_manager.h"
#include "aim/proto/common.pb.h"
#include "aim/ui/crosshair_editor_screen.h"
#include "aim/ui/theme_editor_screen.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace aim {
namespace {

struct KeybindItem {
  std::string label;
  std::string help_text;
  KeyMapping* mapping;
  int is_capturing_index = 0;
};

const std::vector<std::pair<PresentMode, std::string>> kPresentModes{
    {PresentMode::PRESENT_MODE_IMMEDIATE, "Immediate"},
    {PresentMode::PRESENT_MODE_VSYNC, "Vsync"},
    {PresentMode::PRESENT_MODE_MAILBOX, "Mailbox"},
};

const char* kQuickSettingsHelpText =
    "Hold the key to bring up a settings menu which will close when the key is released. Scroll "
    "wheel can be used to adjust mouse sensitivity.";
const char* kAdjustCrosshairSizeHelpText =
    "Hold the key to enable using the scroll wheel to adjust crosshair size.";

class SettingsScreen : public UiScreen {
 public:
  SettingsScreen(Application& app, const std::string& scenario_id)
      : UiScreen(app), updater_(app.settings_manager().CreateUpdater()), scenario_id_(scenario_id) {
    theme_names_ = app.settings_manager().ListThemes();
    crosshair_names_ = app_.settings_manager().ListCrosshairs();
    Settings settings = app_.settings_manager().GetCurrentSettings();

    keybind_items_ = {
        {"Fire", "", updater_.settings.mutable_keybinds()->mutable_fire()},
        {"Restart Scenario", "", updater_.settings.mutable_keybinds()->mutable_restart_scenario()},
        {"Next Scenario", "", updater_.settings.mutable_keybinds()->mutable_next_scenario()},
        {"Edit Scenario", "", updater_.settings.mutable_keybinds()->mutable_edit_scenario()},
        {"Quick Settings",
         kQuickSettingsHelpText,
         updater_.settings.mutable_keybinds()->mutable_quick_settings()},
        {"Quick Metronome", "", updater_.settings.mutable_keybinds()->mutable_quick_metronome()},
        {"Adjust Crosshair Size",
         kAdjustCrosshairSizeHelpText,
         updater_.settings.mutable_keybinds()->mutable_adjust_crosshair_size()},
    };
  }

 protected:
  void DrawSettings() {
    if (!ImGui::BeginTabBar("SettingsTabBar")) {
      return;
    }
    if (ImGui::BeginTabItem("Settings")) {
      ImGui::Spacing();
      ImGui::InputFloat(ImGui::InputFloatParams("CmPer360")
                            .set_label("cm/360")
                            .set_step(1, 5)
                            .set_width(char_x_ * 9)
                            .set_min(1)
                            .set_default(35),
                        PROTO_FLOAT_FIELD(Settings, &updater_.settings, cm_per_360));
      ImGui::SameLine();
      ImGui::HelpMarker("Adjust within a run by holding \"s\" and using the scroll wheel");

      ImGui::InputFloat(ImGui::InputFloatParams("Dpi")
                            .set_label("DPI")
                            .set_step(100, 200)
                            .set_width(char_x_ * 10)
                            .set_min(100)
                            .set_default(800),
                        PROTO_FLOAT_FIELD(Settings, &updater_.settings, dpi));

      ImGui::SpacedSeparator();

      ImGui::InputFloat(ImGui::InputFloatParams("Fps")
                            .set_label("Max render fps")
                            .set_is_optional()
                            .set_step(10, 100)
                            .set_width(char_x_ * 10)
                            .set_range(30, 2000),
                        PROTO_FLOAT_FIELD(Settings, &updater_.settings, max_render_fps));
      ImGui::SameLine();
      ImGui::HelpMarker(
          "State updates and event polling are not tied to fps. A good target can be 2x monitor "
          "refresh rate to reduce tearing.");

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Present mode");
      ImGui::SameLine();
      PresentMode present_mode = updater_.settings.present_mode();
      ImGui::SimpleTypeDropdown("GpuPresentModes", &present_mode, kPresentModes, char_x_ * 10);
      updater_.settings.set_present_mode(present_mode);

      ImGui::SpacedSeparator();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Theme");
      ImGui::SameLine();
      ImGui::SimpleDropdown(
          "ThemeDropdown", updater_.settings.mutable_theme_name(), theme_names_, char_x_ * 20);
      ImGui::SameLine();
      if (ImGui::Button(std::format("{}##EditThemes", icons::kEdit))) {
        PushNextScreen(CreateThemeEditorScreen(&app_));
      }
      ImGui::HelpTooltip("Open theme editor");

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Crosshair");
      ImGui::SameLine();
      bool crosshair_opened = false;
      ImGui::SimpleDropdown("CrosshairDropdown",
                            updater_.settings.mutable_current_crosshair_name(),
                            crosshair_names_,
                            char_x_ * 15,
                            nullptr,
                            &crosshair_opened);
      if (crosshair_opened) {
        crosshair_names_ = app_.settings_manager().ListCrosshairs();
      }
      ImGui::SameLine();
      if (ImGui::Button(std::format("{}##EditCrosshairs", icons::kEdit))) {
        PushNextScreen(CreateCrosshairEditorScreen(&app_));
      }
      ImGui::HelpTooltip("Open crosshair editor");
      ImGui::SameLine();
      if (ImGui::Button(std::format("{}##EditCrosshairColor", icons::kPalette))) {
        ThemeEditorOptions opts;
        opts.selected_theme = updater_.settings.theme_name();
        PushNextScreen(CreateThemeEditorScreen(&app_, opts));
      }
      ImGui::HelpTooltip(
          "Edit the color of the crosshair in the current theme. Crosshair colors are stored with "
          "the theme.");

      ImGui::InputFloat(ImGui::InputFloatParams("CrosshairSize")
                            .set_label("Crosshair size")
                            .set_min(0.1)
                            .set_step(0.1, 1)
                            .set_default(15)
                            .set_width(char_x_ * 9),
                        PROTO_FLOAT_FIELD(Settings, &updater_.settings, crosshair_size));
      ImGui::SameLine();
      ImGui::HelpMarker("Adjust within a run by holding \"c\" and using the scroll wheel");

      ImGui::SpacedSeparator();

      ImGui::InputBool(
          ImGui::InputBoolParams("DisablePerScenarioSettings")
              .set_label("Disable per scenario settings"),
          PROTO_BOOL_FIELD(Settings, &updater_.settings, disable_per_scenario_settings));

      ImGui::InputBool(
          ImGui::InputBoolParams("DisableClickToStart").set_label("Disable \"Click to Start\""),
          PROTO_BOOL_FIELD(Settings, &updater_.settings, disable_click_to_start));

      ImGui::InputBool(ImGui::InputBoolParams("AutoHoldTracking").set_label("Auto hold tracking"),
                       PROTO_BOOL_FIELD(Settings, &updater_.settings, auto_hold_tracking));

      ImGui::InputFloat(ImGui::InputFloatParams("MetronomeBpm")
                            .set_label("Metronome BPM")
                            .set_min(0)
                            .set_zero_is_unset()
                            .set_step(1, 5)
                            .set_width(char_x_ * 10),
                        PROTO_FLOAT_FIELD(Settings, &updater_.settings, metronome_bpm));
      ImGui::SameLine();
      ImGui::HelpMarker("Adjust within a run by holding \"b\" and using the scroll wheel");

      ImGui::SpacedSeparator();

      ImGui::InputBool(
          ImGui::InputBoolParams("ShowHealthBars").set_label("Show health bars"),
          PROTO_BOOL_FIELD(HealthBarSettings, updater_.settings.mutable_health_bar(), show));
      if (updater_.settings.health_bar().show()) {
        ImGui::Indent();

        ImGui::InputBool(
            ImGui::InputBoolParams("OnlyDamaged").set_label("Only damaged"),
            PROTO_BOOL_FIELD(
                HealthBarSettings, updater_.settings.mutable_health_bar(), only_damaged));

        ImGui::InputFloat(
            ImGui::InputFloatParams("HealthBarWidth")
                .set_label("Width")
                .set_min(0.1)
                .set_step(0.1, 1)
                .set_default(6)
                .set_width(char_x_ * 9),
            PROTO_FLOAT_FIELD(HealthBarSettings, updater_.settings.mutable_health_bar(), width));
        ImGui::InputFloat(
            ImGui::InputFloatParams("HealthBarHeight")
                .set_label("Height")
                .set_min(0.1)
                .set_step(0.1, 1)
                .set_default(1.5)
                .set_width(char_x_ * 9),
            PROTO_FLOAT_FIELD(HealthBarSettings, updater_.settings.mutable_health_bar(), height));
        ImGui::InputFloat(
            ImGui::InputFloatParams("HealthBarHeightAboveTarget")
                .set_label("Height above target")
                .set_min(0.1)
                .set_step(0.1, 1)
                .set_default(0.6)
                .set_width(char_x_ * 9),
            PROTO_FLOAT_FIELD(
                HealthBarSettings, updater_.settings.mutable_health_bar(), height_above_target));

        ImGui::Unindent();
      }

      ImGui::SpacedSeparator();

      if (ImGui::Button(std::format("{} Folder", icons::kOpenInNew))) {
        OpenFolderInExplorer(app_.file_system()->GetUserDataPath());
      }
      ImGui::HelpTooltip(
          std::format("Open \"{}\"", app_.file_system()->GetUserDataPath().string()));

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Keybinds")) {
      ImGui::Spacing();
      DrawKeybinds();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Sounds")) {
      ImGui::Spacing();
      DrawSounds();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  void DrawKeybinds() {
    if (ImGui::BeginTable("KeybindsColumns", 2)) {
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, char_x_ * 20);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

      for (KeybindItem& item : keybind_items_) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text(item.label);

        if (item.help_text.size() > 0) {
          ImGui::SameLine();
          ImGui::HelpMarker(item.help_text);
        }

        ImGui::TableNextColumn();
        float entry_width = char_x_ * 10;
        ImGui::SameLine();
        KeyMappingEntry(&item, 1, entry_width);
        ImGui::SameLine();
        KeyMappingEntry(&item, 2, entry_width);
        ImGui::SameLine();
        KeyMappingEntry(&item, 3, entry_width);
        ImGui::SameLine();
        KeyMappingEntry(&item, 4, entry_width);
      }
      ImGui::EndTable();
    }
  }

  void DrawSounds() {
    SoundSettings& s = *updater_.settings.mutable_sound();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Volume level");
    ImGui::SameLine();
    float volume_level = 1;
    if (s.has_master_volume_level()) {
      volume_level = s.master_volume_level();
    }
    ImGui::SliderFloat("##VolumeLevel", &volume_level, 0, 1, "%.2f");
    s.set_master_volume_level(volume_level);

    Line();

    if (ImGui::BeginTable("SoundsColumns", 2)) {
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, char_x_ * 15);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Shoot");
      ImGui::TableNextColumn();
      ImGui::InputText("##ShootSound", s.mutable_shoot());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Hit");
      ImGui::TableNextColumn();
      ImGui::InputText("##HitSound", s.mutable_hit());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Kill");
      ImGui::TableNextColumn();
      ImGui::InputText("##KillSound", s.mutable_kill());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Metronome");
      ImGui::TableNextColumn();
      ImGui::InputText("##MetronomeSound", s.mutable_metronome());

      ImGui::EndTable();
    }
  }

  void DrawScreen() override {
    ImGui::IdGuard cid("SettingsScreen");
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

    DrawTopBar();

    if (BeginMainWindow("MainWindow", 0.8)) {
      DrawSettings();
    }
    ImGui::End();
  }

  void DrawControls() {
    {
      ImVec2 sz = ImVec2(char_x_ * 14, 0.0f);
      if (ImGui::Button("Save", sz)) {
        app_.settings_manager().MarkDirty();
        updater_.SaveIfChangesMade(scenario_id_);
        PopSelf();
      }
    }
    {
      ImGui::SameLine();
      ImVec2 sz = ImVec2(0, 0.0f);
      if (ImGui::Button("Cancel", sz)) {
        PopSelf();
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
  void DrawTopBar() {
    float width = char_x_ * 12.9;
    float middle = app_.screen_info().width / 2.0;
    // ImGui::SetNextWindowBgAlpha();
    ImGui::SetNextWindowPos(ImVec2(middle - width / 2.0, char_x_ / 3.0));
    ImGui::SetNextWindowSize(ImVec2(width, -1));
    ImGui::Begin("TopBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    float start_x = ImGui::GetCursorPosX();

    if (ImGui::Button(std::format("{} Save", icons::kSave))) {
      app_.settings_manager().MarkDirty();
      updater_.SaveIfChangesMade(scenario_id_);
      PopSelf();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      PopSelf();
    }

    ImGui::End();
  }

  bool BeginMainWindow(const std::string& name, float width_multiple) {
    float padding = char_x_ * 0.3;
    float start_y = ImGui::GetCursorPosY() + ImGui::GetTextLineHeight() * 0.9;
    float end_y = app_.screen_info().height - padding;
    float width = app_.screen_info().width * width_multiple;
    float height = end_y - start_y;

    ImGui::SetNextWindowPos(ImVec2((app_.screen_info().width - width) / 2.0, start_y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    return ImGui::Begin(
        name.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
  }

  void Line() {
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();
  }

  SettingsUpdater updater_;
  std::vector<std::string> theme_names_;
  std::vector<std::string> crosshair_names_;
  const std::string scenario_id_;

  std::function<void(const SDL_Event&)> capture_key_fn_;
  std::vector<KeybindItem> keybind_items_;
  float char_x_ = 0;

  int edit_crosshair_index_ = 0;
};
}  // namespace

std::unique_ptr<UiScreen> CreateSettingsScreen(Application* app,
                                               const std::string& current_scenario_id) {
  return std::make_unique<SettingsScreen>(*app, current_scenario_id);
}

}  // namespace aim

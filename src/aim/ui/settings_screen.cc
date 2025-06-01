#include "settings_screen.h"

#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <functional>
#include <optional>

#include "aim/common/field.h"
#include "aim/common/imgui_ext.h"
#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"

namespace aim {
namespace {

const std::vector<std::pair<CrosshairLayer::TypeCase, std::string>> kCrosshairTypes{
    {CrosshairLayer::kDot, "Dot"},
    {CrosshairLayer::kPlus, "Plus"},
    {CrosshairLayer::kCircle, "Circle"},
};

struct KeybindItem {
  std::string label;
  KeyMapping* mapping;
  int is_capturing_index = 0;
};

class SettingsScreen : public UiScreen {
 public:
  SettingsScreen(Application* app, const std::string& scenario_id)
      : UiScreen(app),
        updater_(app->settings_manager(), app->history_db()),
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
        {"Fire", updater_.settings.mutable_keybinds()->mutable_fire()},
        {"Restart Scenario", updater_.settings.mutable_keybinds()->mutable_restart_scenario()},
        {"Next Scenario", updater_.settings.mutable_keybinds()->mutable_next_scenario()},
        {"Edit Scenario", updater_.settings.mutable_keybinds()->mutable_edit_scenario()},
        {"Quick Settings", updater_.settings.mutable_keybinds()->mutable_quick_settings()},
        {"Quick Metronome", updater_.settings.mutable_keybinds()->mutable_quick_metronome()},
        {"Adjust Crosshair Size",
         updater_.settings.mutable_keybinds()->mutable_adjust_crosshair_size()},
    };
  }

 protected:
  void DrawSettings() {
    ImGui::InputJitteredFloat(ImGui::InputFloatParams("CmPer360")
                                  .set_label("cm/360")
                                  .set_step(1, 5)
                                  .set_width(char_x_ * 9)
                                  .set_min(1)
                                  .set_default(35),
                              PROTO_JITTERED_FIELD(Settings, &updater_.settings, cm_per_360));

    ImGui::InputFloat(ImGui::InputFloatParams("Dpi")
                          .set_label("DPI")
                          .set_step(100, 200)
                          .set_width(char_x_ * 10)
                          .set_min(100)
                          .set_default(800),
                      PROTO_FLOAT_FIELD(Settings, &updater_.settings, dpi));

    ImGui::InputFloat(ImGui::InputFloatParams("MetronomeBpm")
                          .set_label("Metronome BPM")
                          .set_min(0)
                          .set_zero_is_unset()
                          .set_step(1, 5)
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(Settings, &updater_.settings, metronome_bpm));

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Theme");
    ImGui::SameLine();
    ImGui::SimpleDropdown(
        "ThemeDropdown", updater_.settings.mutable_theme_name(), theme_names_, char_x_ * 20);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Crosshair");
    ImGui::SameLine();
    ImGui::SimpleDropdown("CrosshairDropdown",
                          updater_.settings.mutable_current_crosshair_name(),
                          crosshair_names_,
                          char_x_ * 15);

    ImGui::InputFloat(ImGui::InputFloatParams("CrosshairSize")
                          .set_label("Crosshair size")
                          .set_min(0.1)
                          .set_step(0.1, 1)
                          .set_precision(1)
                          .set_default(15)
                          .set_width(char_x_ * 9),
                      PROTO_FLOAT_FIELD(Settings, &updater_.settings, crosshair_size));

    ImGui::InputBool(ImGui::InputBoolParams("DisablePerScenarioSettings")
                         .set_label("Disable per scenario settings"),
                     PROTO_BOOL_FIELD(Settings, &updater_.settings, disable_per_scenario_settings));

    ImGui::InputBool(
        ImGui::InputBoolParams("DisableClickToStart").set_label("Disable \"Click to Start\""),
        PROTO_BOOL_FIELD(Settings, &updater_.settings, disable_click_to_start));

    ImGui::InputBool(ImGui::InputBoolParams("AutoHoldTracking").set_label("Auto hold tracking"),
                     PROTO_BOOL_FIELD(Settings, &updater_.settings, auto_hold_tracking));

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
              .set_precision(1)
              .set_default(6)
              .set_width(char_x_ * 9),
          PROTO_FLOAT_FIELD(HealthBarSettings, updater_.settings.mutable_health_bar(), width));
      ImGui::InputFloat(
          ImGui::InputFloatParams("HealthBarHeight")
              .set_label("Height")
              .set_min(0.1)
              .set_step(0.1, 1)
              .set_precision(1)
              .set_default(1.5)
              .set_width(char_x_ * 9),
          PROTO_FLOAT_FIELD(HealthBarSettings, updater_.settings.mutable_health_bar(), height));
      ImGui::InputFloat(
          ImGui::InputFloatParams("HealthBarHeightAboveTarget")
              .set_label("Height above target")
              .set_min(0.1)
              .set_step(0.1, 1)
              .set_precision(1)
              .set_default(0.6)
              .set_width(char_x_ * 9),
          PROTO_FLOAT_FIELD(
              HealthBarSettings, updater_.settings.mutable_health_bar(), height_above_target));

      ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Keybinds");
    ImGui::Indent();
    DrawKeybinds();
    ImGui::Unindent();
  }

  void DrawKeybinds() {
    for (KeybindItem& item : keybind_items_) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text(item.label);
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
  }

  void DrawScreen() override {
    ImGui::IdGuard cid("SettingsScreen");
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

    if (ImGui::Begin("Settings")) {
      DrawSettings();
    }
    ImGui::End();

    if (ImGui::Begin("Crosshairs")) {
      DrawSavedCrosshairsEditor();
    }
    ImGui::End();

    if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoTitleBar)) {
      DrawControls();
    }
    ImGui::End();
  }

  void DrawControls() {
    {
      ImVec2 sz = ImVec2(char_x_ * 14, 0.0f);
      if (ImGui::Button("Save", sz)) {
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

  void DrawSavedCrosshairsEditor() {
    if (updater_.settings.saved_crosshairs_size() == 0) {
      *updater_.settings.add_saved_crosshairs() = GetDefaultCrosshair();
    }
    std::vector<std::string> names;
    for (Crosshair& c : *updater_.settings.mutable_saved_crosshairs()) {
      names.push_back(c.name());
    }
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Crosshair");
    edit_crosshair_index_ = ClampIndex(names, edit_crosshair_index_);
    ImGui::SameLine();
    std::string selected_name = names[edit_crosshair_index_];
    ImGui::SimpleDropdown(
        "CrosshairDropdown", &selected_name, names, char_x_ * 15, &edit_crosshair_index_);
    ImGui::SameLine();
    if (ImGui::Button(kIconAdd)) {
      auto c = GetDefaultCrosshair();
      c.set_name("New crosshair");
      *updater_.settings.add_saved_crosshairs() = c;
      edit_crosshair_index_ = updater_.settings.saved_crosshairs_size() - 1;
    }
    ImGui::HelpTooltip("Add a new saved crosshair");

    if (names.size() > 1) {
      ImGui::SameLine();
      if (ImGui::Button(kIconCancel)) {
        auto* crosshairs = updater_.settings.mutable_saved_crosshairs();
        crosshairs->erase(crosshairs->begin() + edit_crosshair_index_);
        edit_crosshair_index_ = 0;
      }
      ImGui::HelpTooltip("Delete current crosshair");
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();
    DrawCrosshairEditor(*updater_.settings.mutable_saved_crosshairs(edit_crosshair_index_));
  }

  void DrawCrosshairEditor(Crosshair& c) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Name");
    ImGui::SameLine();
    ImGui::InputText("##NameInput", c.mutable_name());

    if (c.layers_size() == 0) {
      c.add_layers();
    }

    ImGui::Text("Layers");
    ImGui::Indent();
    ImGui::LoopId loop_id;
    for (CrosshairLayer& l : *c.mutable_layers()) {
      auto id = loop_id.Get();
      DrawCrosshairLayerEditor(l);
    }
    ImGui::Unindent();

    // Draw the crosshair
    ImVec2 current_pos = ImGui::GetCursorScreenPos();
    ImVec2 available = ImGui::GetContentRegionAvail();

    float height_spacing = ImGui::GetFrameHeight() * 2;
    ImVec2 center = current_pos;
    center.x += (available.x / 2.0f);
    center.y += height_spacing;

    Theme theme;
    *theme.mutable_crosshair()->mutable_color() = ToStoredColor(0.9);
    *theme.mutable_crosshair()->mutable_outline_color() = ToStoredColor(0);

    float w = available.x * 0.9;
    ImVec2 back_min = center;
    ImVec2 back_max = center;

    back_min.x -= w / 2.0;
    back_max.x += w / 2.0;

    back_min.y -= height_spacing;
    back_max.y += height_spacing;

    ImGui::GetWindowDrawList()->AddRectFilled(back_min, back_max, ToImCol32(ToStoredColor(0.3)));
    DrawCrosshair(c, 30, theme, center);
  }

  void DrawCrosshairLayerEditor(CrosshairLayer& l) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Type");
    ImGui::SameLine();
    CrosshairLayer::TypeCase type = l.type_case();
    if (type == CrosshairLayer::TYPE_NOT_SET) {
      type = CrosshairLayer::kDot;
    }
    ImGui::SimpleTypeDropdown("CrosshairType", &type, kCrosshairTypes, char_x_ * 12);

    ImGui::InputFloat(ImGui::InputFloatParams("ScaleInput")
                          .set_label("Scale")
                          .set_step(0.05, 0.2)
                          .set_precision(2)
                          .set_width(char_x_ * 12)
                          .set_default(1)
                          .set_min(0.01),
                      PROTO_FLOAT_FIELD(CrosshairLayer, &l, scale));

    ImGui::InputFloat(ImGui::InputFloatParams("OpacityInput")
                          .set_label("Opacity")
                          .set_step(0.02, 0.2)
                          .set_precision(2)
                          .set_width(char_x_ * 12)
                          .set_default(1)
                          .set_range(0.01, 1),
                      PROTO_FLOAT_FIELD(CrosshairLayer, &l, alpha));

    if (type == CrosshairLayer::kDot) {
      DrawCrosshairDotEditor(l.mutable_dot());
    }
    if (type == CrosshairLayer::kPlus) {
      DrawCrosshairPlusEditor(l.mutable_plus());
    }
    if (type == CrosshairLayer::kCircle) {
      DrawCrosshairCircleEditor(l.mutable_circle());
    }
  }

  void DrawCrosshairDotEditor(DotCrosshair* c) {
    ImGui::InputFloat(ImGui::InputFloatParams("OutlineThicknessInput")
                          .set_label("Outline thickness")
                          .set_step(0.5, 1)
                          .set_precision(1)
                          .set_width(char_x_ * 8)
                          .set_default(1.5)
                          .set_min(0),
                      PROTO_FLOAT_FIELD(DotCrosshair, c, outline_thickness));
  }

  void DrawCrosshairCircleEditor(CircleCrosshair* c) {
    ImGui::IdGuard cid("CircleCrosshair");

    ImGui::InputFloat(ImGui::InputFloatParams("Thickness")
                          .set_step(0.5, 1)
                          .set_precision(1)
                          .set_width(char_x_ * 8)
                          .set_default(1.5)
                          .set_min(0.1),
                      PROTO_FLOAT_FIELD(CircleCrosshair, c, thickness));

    bool use_outline_color = c->use_outline_color();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Use outline color");
    ImGui::SameLine();
    ImGui::Checkbox("##UseOutline", &use_outline_color);
    c->set_use_outline_color(use_outline_color);
  }

  void DrawCrosshairPlusEditor(PlusCrosshair* c) {
    ImGui::IdGuard cid("PlusCrosshair");

    ImGui::InputFloat(ImGui::InputFloatParams("Horizontal size")
                          .set_step(0.1, 0.5)
                          .set_precision(1)
                          .set_width(char_x_ * 8)
                          .set_default(1)
                          .set_min(0),

                      PROTO_FLOAT_FIELD(PlusCrosshair, c, horizontal_size));

    ImGui::InputFloat(ImGui::InputFloatParams("Vertical size")
                          .set_step(0.1, 0.5)
                          .set_precision(1)
                          .set_width(char_x_ * 8)
                          .set_default(1)
                          .set_min(0),
                      PROTO_FLOAT_FIELD(PlusCrosshair, c, vertical_size));

    ImGui::InputFloat(ImGui::InputFloatParams("Thickness")
                          .set_step(0.1, 1)
                          .set_precision(1)
                          .set_width(char_x_ * 10)
                          .set_min(0.1)
                          .set_default(1),
                      PROTO_FLOAT_FIELD(PlusCrosshair, c, thickness));

    ImGui::InputFloat(ImGui::InputFloatParams("Outline thickness")
                          .set_step(0.1, 1)
                          .set_precision(1)
                          .set_width(char_x_ * 8)
                          .set_zero_is_unset(),
                      PROTO_FLOAT_FIELD(PlusCrosshair, c, outline_thickness));

    ImGui::InputFloat(ImGui::InputFloatParams("Rounding")
                          .set_step(0.5, 1)
                          .set_precision(1)
                          .set_width(char_x_ * 8)
                          .set_min(0),
                      PROTO_FLOAT_FIELD(PlusCrosshair, c, rounding));
  }

  void OptionalInputFloat(const std::string& id,
                          bool* has_value,
                          float* value,
                          float step,
                          float fast_step,
                          const char* format = "%.1f") {
    ImGui::OptionalInputFloat(id, has_value, value, step, fast_step, format, char_x_ * 10);
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
  SettingsUpdater updater_;
  SettingsManager* mgr_;
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
  return std::make_unique<SettingsScreen>(app, current_scenario_id);
}

}  // namespace aim

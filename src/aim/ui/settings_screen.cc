#include "settings_screen.h"

#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <functional>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"

namespace aim {
namespace {

template <typename T>
struct FieldFunctions {
  FieldFunctions(std::function<T()> get,
                 std::function<void(T)> set,
                 std::function<void()> clear,
                 std::function<bool()> has)
      : get(std::move(get)), set(std::move(set)), clear(std::move(clear)), has(std::move(has)) {}

  std::function<T()> get;
  std::function<void(T)> set;
  std::function<void()> clear;
  std::function<bool()> has;
};

struct InputFloatParams {
  InputFloatParams(const std::string& name, FieldFunctions<float> field_functions)
      : name(name), field_functions(field_functions) {}

  InputFloatParams& set_precision(int decimal_places) {
    if (decimal_places == 1) {
      format = "%.1f";
    } else if (decimal_places == 2) {
      format = "%.2f";
    } else if (decimal_places == 3) {
      format = "%.2f";
    } else {
      format = "%.0f";
    }
    return *this;
  }

  InputFloatParams& set_step(float step, float fast_step) {
    this->step = step;
    this->fast_step = fast_step;
    return *this;
  }

  InputFloatParams& set_width(float width) {
    this->width = width;
    return *this;
  }

  InputFloatParams& set_default(float default_value) {
    this->default_value = default_value;
    return *this;
  }

  InputFloatParams& set_zero_is_unset() {
    this->zero_is_unset = true;
    return *this;
  }

  InputFloatParams& set_range(float min, float max) {
    min_value = min;
    max_value = max;
    return *this;
  }

  InputFloatParams& set_min(float min) {
    min_value = min;
    return *this;
  }

  InputFloatParams& set_id(const std::string& id) {
    this->id = id;
    return *this;
  }

  std::string name;
  std::string id;
  FieldFunctions<float> field_functions;
  float step = 1;
  float fast_step = 5;
  const char* format = "%.0f";
  float width = -1;
  std::optional<float> default_value;

  std::optional<float> min_value;
  std::optional<float> max_value;
  bool zero_is_unset = false;
};

#define FIELD_FUNCTIONS(T, instance, ProtoClass, field_name)                          \
  FieldFunctions<##T>(std::bind_front(&##ProtoClass::##field_name, ##instance),       \
                      std::bind_front(&##ProtoClass::set_##field_name, ##instance),   \
                      std::bind_front(&##ProtoClass::clear_##field_name, ##instance), \
                      std::bind_front(&##ProtoClass::has_##field_name, ##instance))

void InputFloat(InputFloatParams& params) {
  ImGui::AlignTextToFramePadding();
  ImGui::Text(params.name);
  ImGui::SameLine();
  float value = params.field_functions.get();
  if (params.default_value.has_value() && !params.field_functions.has()) {
    value = *params.default_value;
  }
  if (params.width > 0) {
    ImGui::SetNextItemWidth(params.width);
  }
  std::string id = params.id.size() > 0 ? params.id : std::format("##Input{}", params.name);
  ImGui::InputFloat(id.c_str(), &value, params.step, params.fast_step, params.format);

  if (params.min_value.has_value()) {
    if (value < *params.min_value) {
      value = *params.min_value;
    }
  }
  if (params.max_value.has_value()) {
    if (value > *params.max_value) {
      value = *params.max_value;
    }
  }

  if (params.zero_is_unset) {
    if (value > 0) {
      params.field_functions.set(value);
    } else {
      params.field_functions.clear();
    }
  } else {
    params.field_functions.set(value);
  }
}

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
  bool DisableFullscreenWindow() override {
    return true;
  }

  void DrawSettings() {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("cm/360");
    ImGui::SameLine();
    ImGui::InputJitteredFloat("CmPer360",
                              &settings_updater_.cm_per_360,
                              &settings_updater_.cm_per_360_jitter,
                              1,
                              5,
                              "%.0f",
                              char_x_ * 9);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("DPI");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 10);
    ImGui::InputFloat("##DPI", &settings_updater_.dpi, 100, 200, "%.0f");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Metronome BPM");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 9);
    ImGui::InputFloat("##MetronomeBpm", &settings_updater_.metronome_bpm, 1, 5, "%.0f");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Theme");
    ImGui::SameLine();

    ImGui::SimpleDropdown(
        "ThemeDropdown", &settings_updater_.theme_name, theme_names_, char_x_ * 20);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Crosshair");
    ImGui::SameLine();
    ImGui::SimpleDropdown(
        "CrosshairDropdown", &settings_updater_.crosshair_name, crosshair_names_, char_x_ * 15);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Crosshair Size");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 9);
    ImGui::InputFloat("##CrosshairSize", &settings_updater_.crosshair_size, 0.1, 1, "%.1f");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Disable \"Click to Start\"");
    ImGui::SameLine();
    ImGui::Checkbox("##disable_click_to_start", &settings_updater_.disable_click_to_start);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Auto Hold Tracking");
    ImGui::SameLine();
    ImGui::Checkbox("##auto_hold_tracking", &settings_updater_.auto_hold_tracking);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Show health bars");
    ImGui::SameLine();
    bool show_health_bars = settings_updater_.health_bar.show();
    ImGui::Checkbox("##HealthBarCheckbox", &show_health_bars);
    settings_updater_.health_bar.set_show(show_health_bars);
    if (show_health_bars) {
      ImGui::Indent();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Only damaged");
      ImGui::SameLine();
      bool only_damaged = settings_updater_.health_bar.only_damaged();
      ImGui::Checkbox("##HealthBarDamaged", &only_damaged);
      settings_updater_.health_bar.set_only_damaged(only_damaged);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Width");
      ImGui::SameLine();
      float bar_width = FirstGreaterThanZero(settings_updater_.health_bar.width(), 6);
      ImGui::SetNextItemWidth(char_x_ * 9);
      ImGui::InputFloat("##HealthBarWidth", &bar_width, 0.1, 1, "%.1f");
      if (bar_width <= 0) {
        bar_width = 0.1;
      }
      settings_updater_.health_bar.set_width(bar_width);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Height");
      ImGui::SameLine();
      float bar_height = FirstGreaterThanZero(settings_updater_.health_bar.height(), 1.5);
      ImGui::SetNextItemWidth(char_x_ * 9);
      ImGui::InputFloat("##HealthBarHeight", &bar_height, 0.1, 1, "%.1f");
      if (bar_height <= 0) {
        bar_height = 0.1;
      }
      settings_updater_.health_bar.set_height(bar_height);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Height above target");
      ImGui::SameLine();
      float height_above =
          FirstGreaterThanZero(settings_updater_.health_bar.height_above_target(), 0.6);
      ImGui::SetNextItemWidth(char_x_ * 9);
      ImGui::InputFloat("##HeightAbove", &height_above, 0.1, 1, "%.1f");
      if (height_above <= 0) {
        height_above = 0.1;
      }
      settings_updater_.health_bar.set_height_above_target(height_above);

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

  void DrawSavedCrosshairsEditor() {
    if (settings_updater_.saved_crosshairs.crosshairs_size() == 0) {
      *settings_updater_.saved_crosshairs.add_crosshairs() = GetDefaultCrosshair();
    }
    std::vector<std::string> names;
    for (Crosshair& c : *settings_updater_.saved_crosshairs.mutable_crosshairs()) {
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
      *settings_updater_.saved_crosshairs.add_crosshairs() = c;
      edit_crosshair_index_ = settings_updater_.saved_crosshairs.crosshairs_size() - 1;
    }
    ImGui::HelpTooltip("Add a new saved crosshair");

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();
    DrawCrosshairEditor(
        *settings_updater_.saved_crosshairs.mutable_crosshairs(edit_crosshair_index_));
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

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scale");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 12);
    float scale = l.scale();
    ImGui::InputFloat("##ScaleInput", &scale, 0.05, 0.2, "%.2f");
    if (scale > 0) {
      l.set_scale(scale);
    } else {
      l.clear_scale();
    }

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
    InputFloatParams outline_thickness("Outline thickness",
                                       FIELD_FUNCTIONS(float, c, DotCrosshair, outline_thickness));
    outline_thickness.set_step(0.5, 1)
        .set_precision(1)
        .set_width(char_x_ * 8)
        .set_default(1.5)
        .set_min(0);
    InputFloat(outline_thickness);
  }

  void DrawCrosshairCircleEditor(CircleCrosshair* c) {
    InputFloatParams thickness("Thickness", FIELD_FUNCTIONS(float, c, CircleCrosshair, thickness));
    thickness.set_step(0.5, 1)
        .set_precision(1)
        .set_width(char_x_ * 8)
        .set_default(1.5)
        .set_min(0.1);
    InputFloat(thickness);
  }

  void DrawCrosshairPlusEditor(PlusCrosshair* c) {
    ImGui::IdGuard cid("PlusCrosshair");

    InputFloatParams horizontal_size("Horizontal size",
                                     FIELD_FUNCTIONS(float, c, PlusCrosshair, horizontal_size));
    horizontal_size.set_step(0.1, 0.5)
        .set_precision(1)
        .set_width(char_x_ * 8)
        .set_default(1)
        .set_min(0);
    InputFloat(horizontal_size);

    InputFloatParams vertical_size("Vertical size",
                                   FIELD_FUNCTIONS(float, c, PlusCrosshair, vertical_size));
    vertical_size.set_step(0.1, 0.5)
        .set_precision(1)
        .set_width(char_x_ * 8)
        .set_default(1)
        .set_min(0);
    InputFloat(vertical_size);

    InputFloatParams thickness("Thickness", FIELD_FUNCTIONS(float, c, PlusCrosshair, thickness));
    thickness.set_step(0.1, 1)
        .set_precision(1)
        .set_width(char_x_ * 10)
        .set_min(0.1)
        .set_default(1);
    InputFloat(thickness);

    InputFloatParams outline_thickness("Outline thickness",
                                       FIELD_FUNCTIONS(float, c, PlusCrosshair, outline_thickness));
    outline_thickness.set_step(0.1, 1)
        .set_precision(1)
        .set_width(char_x_ * 8)
        .set_zero_is_unset();
    InputFloat(outline_thickness);

    InputFloatParams rounding("Rounding", FIELD_FUNCTIONS(float, c, PlusCrosshair, rounding));
    rounding.set_step(0.5, 1).set_precision(1).set_width(char_x_ * 8).set_min(0);
    InputFloat(rounding);
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
  SettingsUpdater settings_updater_;
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

#pragma once

#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "aim/common/field.h"
#include "aim/common/mat_icons.h"
#include "aim/common/util.h"
#include "aim/proto/common.pb.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace ImGui {

// ImGui::Text but taking same arguments as std::format to product the text.
template <class... _Types>
static void TextFmt(const std::format_string<_Types...> fmt, _Types&&... args) {
  std::string message = std::format(fmt, std::forward<_Types>(args)...);
  Text("%s", message.c_str());
}

// ImGui::TextDisabled but taking same arguments as std::format to product the text.
template <class... _Types>
static void TextDisabledFmt(const std::format_string<_Types...> fmt, _Types&&... args) {
  std::string message = std::format(fmt, std::forward<_Types>(args)...);
  TextDisabled("%s", message.c_str());
}

struct IdGuard {
  IdGuard(std::string id) {
    ImGui::PushID(id.c_str());
  }

  IdGuard(std::string prefix, int num) {
    ImGui::PushID(std::format("{}{}", prefix, num).c_str());
  }

  IdGuard(int id) {
    ImGui::PushID(id);
  }

  ~IdGuard() {
    Pop();
  }

  void Pop() {
    if (!closed) {
      ImGui::PopID();
      closed = true;
    }
  }

  IdGuard(const IdGuard&) = delete;
  IdGuard& operator=(const IdGuard&) = delete;

 private:
  bool closed = false;
};

struct LoopId {
  LoopId() {}

  IdGuard Get() {
    return IdGuard(++i);
  }

  IdGuard Get(const std::string& prefix) {
    return IdGuard(prefix, ++i);
  }

  int i = -1;
};

void TextDisabled(const std::string& val);
void TextWrapped(const std::string& val);

void Text(const std::string& val);

bool Button(const std::string& label, const ImVec2& size = ImVec2(0, 0));

bool SimpleDropdown(const std::string& id,
                    std::string* value,
                    const std::vector<std::string>& values,
                    float input_width = -1,
                    int* selected_index = nullptr,
                    bool* opened = nullptr);

template <typename T>
bool SimpleTypeDropdown(const std::string& id,
                        T* value,
                        const std::vector<std::pair<T, std::string>>& values,
                        float input_width = -1) {
  ImGui::IdGuard cid(id);
  if (input_width > 0) {
    ImGui::PushItemWidth(input_width);
  }
  std::string initial_value;
  for (auto& v : values) {
    if (v.first == *value) {
      initial_value = v.second;
      break;
    }
  }

  bool item_was_selected = false;
  ImGuiComboFlags combo_flags = ImGuiComboFlags_HeightLarge;
  if (ImGui::BeginCombo("##Combo", initial_value.c_str(), combo_flags)) {
    ImGui::LoopId loop_id;
    for (const auto& item : values) {
      auto id = loop_id.Get();
      bool is_selected = item.first == *value;
      if (ImGui::Selectable(item.second.c_str(), is_selected)) {
        *value = item.first;
        item_was_selected = true;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  if (input_width > 0) {
    ImGui::PopItemWidth();
  }
  return item_was_selected;
}

template <typename T>
class ConfirmationDialog {
 public:
  explicit ConfirmationDialog(const std::string& id) : id_(id) {}

  void NotifyOpen(const std::string& text, const T& value) {
    open_ = true;
    data_ = value;
    text_ = text;
  }

  template <typename ConfirmFn>
  void Draw(const std::string& confirm_text, ConfirmFn&& confirm_fn) {
    bool show_popup = data_.has_value();
    if (show_popup) {
      ImGui::SetNextWindowPos(
          ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
      if (ImGui::BeginPopupModal(id_.c_str(),
                                 &show_popup,
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::Text(text_);

        float button_width = ImGui::CalcTextSize("OK").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - button_width) * 0.5f);

        if (ImGui::Button(confirm_text.c_str())) {
          confirm_fn(*data_);
          data_ = {};
          text_ = "";
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          data_ = {};
          text_ = "";
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
    if (open_) {
      ImGui::OpenPopup(id_.c_str());
      open_ = false;
    }
  }

 private:
  std::optional<T> data_{};
  bool open_ = false;
  std::string id_;
  std::string text_;
};

class NotificationPopup {
 public:
  explicit NotificationPopup(const std::string& id) : id_(id) {}

  void NotifyOpen(const std::string& text) {
    open_ = true;
    text_ = text;
  }

  void Draw();

 private:
  bool open_ = false;
  std::string id_;
  std::string text_;
};

void HelpMarker(const std::string& text);
void InfoMarker(const std::string& text);
void HelpTooltip(const std::string& text);

void DrawItemBounds();
void SetCursorAtBottom(float item_height = -1);
void SetCursorAtRight(float item_width);
void SetButtonCursorAtRight(const std::string& text);

struct InputFloatParams {
  explicit InputFloatParams(const std::string& id) : id(id) {}

  static InputFloatParams WithLabelAsId(const std::string& label) {
    return InputFloatParams(label).set_label(label);
  }

  InputFloatParams clone() const {
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

  InputFloatParams& set_max(float max) {
    max_value = max;
    return *this;
  }

  InputFloatParams& set_label(const std::string& label) {
    this->label = label;
    return *this;
  }

  InputFloatParams& set_id(const std::string& id) {
    this->id = id;
    return *this;
  }

  InputFloatParams& set_id_and_label(const std::string& label) {
    this->id = label;
    this->label = label;
    return *this;
  }

  InputFloatParams& set_is_optional() {
    is_optional = true;
    return *this;
  }

  InputFloatParams& set_is_optional(bool value) {
    is_optional = value;
    return *this;
  }

  std::string id;
  std::string label;

  float step = 1;
  float fast_step = 5;
  const char* format = "%.6g";
  float width = -1;
  std::optional<float> default_value;

  std::optional<float> min_value;
  std::optional<float> max_value;
  bool zero_is_unset = false;
  bool is_optional = false;
};

void InputFloat(const InputFloatParams& params, aim::Field<float> field);

void InputJitteredFloat(const InputFloatParams& params, aim::JitteredField<float> field);

struct InputBoolParams {
  explicit InputBoolParams(const std::string& id) : id(id) {}

  InputBoolParams& set_label(const std::string& label) {
    this->label = label;
    return *this;
  }

  InputBoolParams& set_false_is_unset() {
    false_is_unset = true;
    return *this;
  }

  std::string id;
  std::string label;
  bool false_is_unset = false;
};

void InputBool(const InputBoolParams& params, aim::Field<bool> field);

struct InputIntParams {
  explicit InputIntParams(const std::string& id) : id(id) {}

  static InputIntParams WithLabelAsId(const std::string& label) {
    return InputIntParams(label).set_label(label);
  }

  InputIntParams& set_step(int step, int fast_step) {
    this->step = step;
    this->fast_step = fast_step;
    return *this;
  }

  InputIntParams& set_width(float width) {
    this->width = width;
    return *this;
  }

  InputIntParams& set_default(int default_value) {
    this->default_value = default_value;
    return *this;
  }

  InputIntParams& set_zero_is_unset() {
    this->zero_is_unset = true;
    return *this;
  }

  InputIntParams& set_range(int min, int max) {
    min_value = min;
    max_value = max;
    return *this;
  }

  InputIntParams& set_min(int min) {
    min_value = min;
    return *this;
  }

  InputIntParams& set_label(const std::string& label) {
    this->label = label;
    return *this;
  }

  InputIntParams& set_id(const std::string& id) {
    this->id = id;
    return *this;
  }

  InputIntParams& set_is_optional() {
    is_optional = true;
    return *this;
  }

  std::string id;
  std::string label;

  int step = 1;
  int fast_step = 5;
  float width = -1;
  std::optional<int> default_value;

  std::optional<int> min_value;
  std::optional<int> max_value;
  bool zero_is_unset = false;
  bool is_optional = false;
};

void InputInt(const InputIntParams& params, aim::Field<int> field);

void InputStoredColor(const std::string& id, aim::StoredColor* stored_color, float char_x);

void SpacedSeparator();

float GetDefaultCharSizeX();

void ReadonlyLabeledFloat(const std::string& label, float value, int width_multiple = 8);

bool Chip(const std::string& label, bool selected);

template <typename T>
bool ChipSelector(const std::string& id,
                  T* current_value,
                  const std::vector<std::pair<T, std::string>>& values) {
  ImGui::IdGuard cid(id);

  T selected_value = *current_value;
  bool is_first = true;
  for (auto& entry : values) {
    T value_type = entry.first;
    const std::string& label = entry.second;
    if (is_first) {
      is_first = false;
    } else {
      ImGui::SameLine();
    }
    if (Chip(label.c_str(), *current_value == value_type)) {
      selected_value = value_type;
    }
  }

  bool item_was_changed = selected_value != *current_value;
  *current_value = selected_value;
  return item_was_changed;
}

bool SelectableButton(const std::string& label);

bool BeginDefaultPopupModal(const char* id, bool* draw);

}  // namespace ImGui

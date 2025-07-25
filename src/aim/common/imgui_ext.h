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

static void Text(const std::string& val) {
  Text("%s", val.c_str());
}

static bool Button(const std::string& label, const ImVec2& size = ImVec2(0, 0)) {
  return Button(label.c_str(), size);
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

static void OptionalInputFloat(const std::string& id,
                               bool* has_value,
                               float* value,
                               float step,
                               float fast_step,
                               const char* format,
                               float input_width) {
  ImGui::IdGuard cid(id);
  ImGui::Checkbox("##HasValue", has_value);
  if (*has_value) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(input_width);
    ImGui::InputFloat("##ValueInput", value, step, fast_step, format);
  }
}

static void InputJitteredFloat(const std::string& id,
                               float* value,
                               float* jitter_value,
                               float step,
                               float fast_step,
                               const char* format,
                               float input_width) {
  ImGui::IdGuard cid(id);
  ImGui::SetNextItemWidth(input_width);
  ImGui::InputFloat("##ValueEntry", value, step, fast_step, format);

  ImGui::SameLine();
  ImGui::Text("+/-");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(input_width);
  ImGui::InputFloat("##JitterValueEntry", jitter_value, step, fast_step, format);
  if (*jitter_value < 0) {
    *jitter_value = 0;
  }
}

static bool SimpleDropdown(const std::string& id,
                           std::string* value,
                           const std::vector<std::string>& values,
                           float input_width = -1,
                           int* selected_index = nullptr,
                           bool* opened = nullptr) {
  ImGui::IdGuard cid(id);
  if (input_width > 0) {
    ImGui::PushItemWidth(input_width);
  }
  bool item_was_selected = false;
  ImGuiComboFlags combo_flags = ImGuiComboFlags_HeightLarge;
  if (opened != nullptr) {
    *opened = false;
  }
  if (ImGui::BeginCombo("##Combo", value->c_str(), combo_flags)) {
    if (opened != nullptr) {
      *opened = true;
    }
    for (int i = 0; i < values.size(); ++i) {
      ImGui::IdGuard lid(i);
      const auto& item = values[i];
      bool is_selected = item == *value;
      if (ImGui::Selectable(item.c_str(), is_selected)) {
        *value = item;
        item_was_selected = true;
        if (selected_index != nullptr) {
          *selected_index = i;
        }
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

  void Draw() {
    bool show_popup = text_.size() > 0;
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

        if (ImGui::Button("Ok")) {
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
  bool open_ = false;
  std::string id_;
  std::string text_;
};

static void HelpMarker(const std::string& text) {
  ImGui::TextDisabled(aim::kIconHelp);
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::Text(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

static void HelpTooltip(const std::string& text) {
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::Text(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

static void DrawItemBounds() {
  ImVec2 rect_min = ImGui::GetItemRectMin();
  ImVec2 rect_max = ImGui::GetItemRectMax();

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddLine(ImVec2(rect_min.x, rect_min.y),
                     ImVec2(rect_max.x, rect_min.y),
                     ImGui::GetColorU32(ImGuiCol_DragDropTarget),
                     2.0f);
  draw_list->AddLine(ImVec2(rect_min.x, rect_max.y),
                     ImVec2(rect_max.x, rect_max.y),
                     ImGui::GetColorU32(ImGuiCol_DragDropTarget),
                     2.0f);
}

static void SetCursorAtBottom(float item_height = -1) {
  if (item_height < 0) {
    item_height = ImGui::GetFrameHeight();
  }
  float content_region_avail_height = ImGui::GetContentRegionAvail().y;
  float bottom_target_y = ImGui::GetCursorPosY() + content_region_avail_height - item_height;
  ImGui::SetCursorPosY(bottom_target_y);
}

static void SetCursorAtRight(float item_width) {
  float available = ImGui::GetContentRegionAvail().x;
  float target_x = ImGui::GetCursorPosX() + available - item_width;
  ImGui::SetCursorPosX(target_x);
}

static void SetButtonCursorAtRight(const std::string& text) {
  float size = ImGui::CalcTextSize(text.c_str()).x;
  SetCursorAtRight(size + 2.0f * ImGui::GetStyle().FramePadding.x);
}

struct InputFloatParams {
  explicit InputFloatParams(const std::string& id) : id(id) {}

  static InputFloatParams WithLabelAsId(const std::string& label) {
    return InputFloatParams(label).set_label(label);
  }

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

  InputFloatParams& set_is_optional() {
    is_optional = true;
    return *this;
  }

  std::string id;
  std::string label;

  float step = 1;
  float fast_step = 5;
  const char* format = "%.0f";
  float width = -1;
  std::optional<float> default_value;

  std::optional<float> min_value;
  std::optional<float> max_value;
  bool zero_is_unset = false;
  bool is_optional = false;
};

static void InputFloat(const InputFloatParams& params, aim::Field<float> field) {
  IdGuard cid(params.id);
  if (params.label.size() > 0) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text(params.label);
    ImGui::SameLine();
  }
  if (params.is_optional) {
    bool has_field = field.has();
    ImGui::Checkbox("##HasField", &has_field);
    if (!has_field) {
      field.clear();
      return;
    }
    ImGui::SameLine();
  }

  float value = field.get();
  if (params.default_value.has_value() && !field.has()) {
    value = *params.default_value;
  }
  if (params.width > 0) {
    ImGui::SetNextItemWidth(params.width);
  }
  ImGui::InputFloat("##ValueInput", &value, params.step, params.fast_step, params.format);

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
      field.set(value);
    } else {
      field.clear();
    }
  } else {
    field.set(value);
  }
}

static void InputJitteredFloat(const InputFloatParams& params, aim::JitteredField<float> field) {
  InputFloat(params, field.value);

  ImGui::SameLine();
  ImGui::Text("+/-");

  ImGui::SameLine();

  InputFloatParams jitter_params(params.id + "JitterInput");
  jitter_params.set_label("")
      .set_min(0)
      .set_step(params.step, params.fast_step)
      .set_width(params.width);
  jitter_params.format = params.format;

  InputFloat(jitter_params, field.jitter);
}

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

static void InputBool(const InputBoolParams& params, aim::Field<bool> field) {
  IdGuard cid(params.id);
  if (params.label.size() > 0) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text(params.label);
    ImGui::SameLine();
  }

  bool value = field.get();
  ImGui::Checkbox("##Checkbox", &value);
  if (params.false_is_unset) {
    if (value) {
      field.set(value);
    } else {
      field.clear();
    }
  } else {
    field.set(value);
  }
}

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

static void InputInt(const InputIntParams& params, aim::Field<int> field) {
  IdGuard cid(params.id);
  if (params.label.size() > 0) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text(params.label);
    ImGui::SameLine();
  }
  if (params.is_optional) {
    bool has_field = field.has();
    ImGui::Checkbox("##HasField", &has_field);
    if (!has_field) {
      field.clear();
      return;
    }
    ImGui::SameLine();
  }

  int value = field.get();
  if (params.default_value.has_value() && !field.has()) {
    value = *params.default_value;
  }
  if (params.width > 0) {
    ImGui::SetNextItemWidth(params.width);
  }
  ImGui::InputInt("##ValueInput", &value, params.step, params.fast_step);

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
      field.set(value);
    } else {
      field.clear();
    }
  } else {
    field.set(value);
  }
}

static void InputStoredColor(const std::string& id, aim::StoredColor* stored_color, float char_x) {
  ImGui::IdGuard cid(id);

  float color[3];
  aim::StoredRgb c = ToStoredRgb(*stored_color);
  color[0] = c.r() / 255.0;
  color[1] = c.g() / 255.0;
  color[2] = c.b() / 255.0;
  if (ImGui::ColorEdit3(
          "##ColorEditor", color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
    aim::StoredRgb result = aim::FloatToStoredRgb(color[0], color[1], color[2]);
    if (stored_color->has_hex()) {
      stored_color->set_hex(ToHexString(result));
      stored_color->clear_r();
      stored_color->clear_b();
      stored_color->clear_g();
    } else {
      stored_color->set_r(result.r());
      stored_color->set_g(result.g());
      stored_color->set_b(result.b());
    }
  }

  ImGui::SameLine();
  ImGui::Text(aim::kIconClose);
  ImGui::HelpTooltip("Multiply color by value");

  ImGui::SameLine();
  ImGui::InputFloat(ImGui::InputFloatParams("ColorMultiplier")
                        .set_step(0.01, 0.2)
                        .set_precision(2)
                        .set_max(2)
                        .set_width(char_x * 10)
                        .set_zero_is_unset(),
                    PROTO_FLOAT_FIELD(aim::StoredColor, stored_color, multiplier));
}

}  // namespace ImGui

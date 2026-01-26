#include "imgui_ext.h"

namespace ImGui {

void TextDisabled(const std::string& val) {
  TextDisabled("%s", val.c_str());
}

void Text(const std::string& val) {
  Text("%s", val.c_str());
}

bool Button(const std::string& label, const ImVec2& size) {
  return Button(label.c_str(), size);
}

void OptionalInputFloat(const std::string& id,
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

void InputJitteredFloat(const std::string& id,
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

bool SimpleDropdown(const std::string& id,
                    std::string* value,
                    const std::vector<std::string>& values,
                    float input_width,
                    int* selected_index,
                    bool* opened) {
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

void NotificationPopup::Draw() {
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

void HelpMarker(const std::string& text) {
  ImGui::TextDisabled("%s", aim::icons::kHelp);
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::Text(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void HelpTooltip(const std::string& text) {
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::Text(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void DrawItemBounds() {
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

void SetCursorAtBottom(float item_height) {
  if (item_height < 0) {
    item_height = ImGui::GetFrameHeight();
  }
  float content_region_avail_height = ImGui::GetContentRegionAvail().y;
  float bottom_target_y = ImGui::GetCursorPosY() + content_region_avail_height - item_height;
  ImGui::SetCursorPosY(bottom_target_y);
}

void SetCursorAtRight(float item_width) {
  float available = ImGui::GetContentRegionAvail().x;
  float target_x = ImGui::GetCursorPosX() + available - item_width;
  ImGui::SetCursorPosX(target_x);
}

void SetButtonCursorAtRight(const std::string& text) {
  float size = ImGui::CalcTextSize(text.c_str()).x;
  SetCursorAtRight(size + 2.0f * ImGui::GetStyle().FramePadding.x);
}

void InputFloat(const InputFloatParams& params, aim::Field<float> field) {
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

void InputJitteredFloat(const InputFloatParams& params, aim::JitteredField<float> field) {
  InputFloat(params, field.value);

  InputFloatParams jitter_params(params.id + "JitterInput");
  jitter_params.set_label("")
      .set_min(0)
      .set_is_optional(false)
      .set_step(params.step, params.fast_step)
      .set_width(params.width);
  jitter_params.format = params.format;

  if (!params.is_optional || field.value.has()) {
    ImGui::SameLine();
    ImGui::Text("+/-");
    ImGui::SameLine();
    InputFloat(jitter_params, field.jitter);
  } else {
    field.jitter.clear();
  }
}

void InputBool(const InputBoolParams& params, aim::Field<bool> field) {
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

void InputInt(const InputIntParams& params, aim::Field<int> field) {
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

void InputStoredColor(const std::string& id, aim::StoredColor* stored_color, float char_x) {
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
  ImGui::Text("%s", aim::icons::kClose);
  ImGui::HelpTooltip("Multiply color by value");

  ImGui::SameLine();
  ImGui::InputFloat(ImGui::InputFloatParams("ColorMultiplier")
                        .set_step(0.01, 0.2)
                        .set_max(2)
                        .set_width(char_x * 10)
                        .set_zero_is_unset(),
                    PROTO_FLOAT_FIELD(aim::StoredColor, stored_color, multiplier));
}

void SpacedSeparator() {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
}

float GetDefaultCharSizeX() {
  return ImGui::CalcTextSize("A").x;
}

void ReadonlyLabeledFloat(const std::string& label, float value, int width_multiple) {
  IdGuard cid("RoFloat_" + label);
  ImGui::AlignTextToFramePadding();
  ImGui::Text(label);
  ImGui::SameLine();
  float char_x = GetDefaultCharSizeX();
  ImGui::SetNextItemWidth(char_x * width_multiple);
  ImGui::InputFloat("##ValueOut", &value, 0.0f, 0.0f, "%.3g", ImGuiInputTextFlags_ReadOnly);
}

bool Chip(const std::string& label, bool selected) {
  ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));

  ImVec2 size = ImGui::CalcTextSize(label.c_str());
  ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
  bool clicked = false;
  if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None, size)) {
    clicked = true;
  }

  ImGui::PopStyleVar();
  return clicked;
}

bool SelectableButton(const std::string& label) {
  ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));

  ImVec2 size = ImGui::CalcTextSize(label.c_str());
  bool clicked = false;
  if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_None, size)) {
    clicked = true;
  }

  ImGui::PopStyleVar();
  return clicked;
}

}  // namespace ImGui

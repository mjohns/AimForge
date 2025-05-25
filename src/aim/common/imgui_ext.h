#pragma once

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <string>
#include <vector>

#include "aim/common/mat_icons.h"

namespace ImGui {

// ImGui::Text but taking same arguments as std::format to product the text.
template <class... _Types>
static void TextFmt(const std::format_string<_Types...> fmt, _Types&&... args) {
  std::string message = std::format(fmt, std::forward<_Types>(args)...);
  Text("%s", message.c_str());
}

static void Text(const std::string& val) {
  Text("%s", val.c_str());
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
                           float input_width = -1) {
  ImGui::IdGuard cid(id);
  if (input_width > 0) {
    ImGui::PushItemWidth(input_width);
  }
  bool item_was_selected = false;
  ImGuiComboFlags combo_flags = 0;
  if (ImGui::BeginCombo("##Combo", value->c_str(), combo_flags)) {
    ImGui::LoopId loop_id;
    for (const auto& item : values) {
      auto id = loop_id.Get();
      bool is_selected = item == *value;
      if (ImGui::Selectable(item.c_str(), is_selected)) {
        *value = item;
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
  ImGuiComboFlags combo_flags = 0;
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

}  // namespace ImGui

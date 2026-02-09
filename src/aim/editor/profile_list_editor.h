#pragma once

#include <string>

#include "absl/strings/ascii.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/proto_util.h"
#include "aim/proto/scenario.pb.h"
#include "google/protobuf/message.h"
#include "imgui.h"

namespace aim {

template <typename T, typename DrawFn>
void DrawProfileList(const std::string& id,
                     const std::string& type_name,
                     google::protobuf::RepeatedField<int>* order_list,
                     google::protobuf::RepeatedPtrField<T>* profile_list,
                     DrawFn&& draw_profile_fn) {
  ImGui::IdGuard cid(id);
  float char_x = ImGui::GetDefaultCharSizeX();

  std::string lower_type_name = absl::AsciiStrToLower(type_name);
  ImGui::AlignTextToFramePadding();
  ImGui::TextFmt("Explicit {} selection order", lower_type_name);
  bool use_order = order_list->size() > 0;
  ImGui::SameLine();
  ImGui::Checkbox("##UseOrder", &use_order);
  ImGui::SameLine();
  ImGui::HelpMarker(
      "Specify the order in which profiles should be selected. 0, 1 means alternate between "
      "first and second profile");
  if (use_order) {
    ImGui::Indent();
    if (order_list->size() == 0) {
      order_list->Add(0);
    }
    int remove_at_i = -1;
    for (int i = 0; i < order_list->size(); ++i) {
      ImGui::IdGuard lid("Order", i);
      u32 number = order_list->at(i);
      u32 step = 1;
      ImGui::AlignTextToFramePadding();
      ImGui::Text(type_name);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(char_x * 8);
      ImGui::InputScalar("##OrderItemInput", ImGuiDataType_U32, &number, &step, nullptr, "%u");
      number = std::min<u32>(number, profile_list->size() - 1);
      order_list->Set(i, number);

      auto last_size = ImGui::GetItemRectSize();

      ImGui::SameLine();
      if (ImGui::SelectableButton(icons::kClear)) {
        remove_at_i = i;
      }

      if (IsValidIndex(*profile_list, number)) {
        auto& profile = profile_list->at(number);
        if (profile.info().description().size() > 0) {
          ImGui::SameLine();
          ImGui::TextDisabled("%s", profile.info().description().c_str());
        }
      }
    }
    if (ImGui::Button("Add##Order")) {
      order_list->Add(0);
    }
    if (remove_at_i >= 0) {
      order_list->erase(order_list->begin() + remove_at_i);
    }
    ImGui::Unindent();
  } else {
    order_list->Clear();
  }

  bool use_weights = order_list->size() == 0 && profile_list->size() > 1;
  int remove_at_i = -1;
  int move_up_i = -1;
  int move_down_i = -1;
  int copy_i = -1;

  float total_weight = 0;
  for (int i = 0; i < profile_list->size(); ++i) {
    auto* p = &profile_list->at(i);
    total_weight += p->info().weight();
  }

  for (int i = 0; i < profile_list->size(); ++i) {
    ImGui::IdGuard lid(type_name, i);
    auto* p = &profile_list->at(i);
    ImGui::AlignTextToFramePadding();
    ImGui::TextFmt("{} #{}", type_name, i);
    const char* item_menu_id = "profile_item_menu";
    if (ImGui::BeginPopupContextItem(item_menu_id)) {
      if (ImGui::Selectable("Move up")) {
        move_up_i = i;
      }
      if (ImGui::Selectable("Move down")) {
        move_down_i = i;
      }
      if (ImGui::Selectable("Copy")) {
        copy_i = i;
      }
      if (ImGui::Selectable("Delete")) {
        remove_at_i = i;
      }
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick(item_menu_id, ImGuiPopupFlags_MouseButtonRight);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x * 22);
    ImGui::InputText("##DescriptionInput", p->mutable_info()->mutable_description());
    ImGui::SameLine();
    if (ImGui::SelectableButton(icons::kMoreVert)) {
      ImGui::OpenPopup(item_menu_id);
    }

    ImGui::Indent();
    if (use_weights) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Selection weight");
      ImGui::SameLine();
      int weight = p->info().weight();
      if (!p->info().has_weight()) {
        weight = 1;
      }
      ImGui::SetNextItemWidth(char_x * 10);
      ImGui::InputInt("##WeightInput", &weight, 1, 5);
      p->mutable_info()->set_weight(weight);

      if (total_weight > 0) {
        ImGui::SameLine();
        float weight_percent = (weight / total_weight) * 100;
        ImGui::TextDisabled("%.0f%%", weight_percent);
      }

      ImGui::InputInt(ImGui::InputIntParams("NextProfile")
                          .set_label("Next profile")
                          .set_step(1, 2)
                          .set_min(1)
                          .set_default(1)
                          .set_is_optional()
                          .set_width(char_x * 10),
                      PROTO_INT_FIELD(ProfileInfo, p->mutable_info(), next_profile));
      ImGui::SameLine();
      ImGui::HelpMarker("If this profile is selected, always select the specified profile next.");

      ImGui::InputInt(ImGui::InputIntParams("MinSelectionGap")
                          .set_label("Selection gap")
                          .set_step(1, 2)
                          .set_min(1)
                          .set_default(2)
                          .set_is_optional()
                          .set_width(char_x * 10),
                      PROTO_INT_FIELD(ProfileInfo, p->mutable_info(), min_selection_gap));
      ImGui::SameLine();
      ImGui::HelpMarker(
          "Limit how frequently the profile can be selected. A value of 2 means that 2 other "
          "profiles must be selected before this one can be chosen again.");

    } else {
      p->mutable_info()->clear_weight();
      p->mutable_info()->clear_next_profile();
    }

    if (IsDefaultInstance(p->info())) {
      p->clear_info();
    }

    draw_profile_fn(&profile_list->at(i));
    ImGui::Unindent();
  }

  if (remove_at_i >= 0) {
    profile_list->erase(profile_list->begin() + remove_at_i);
  } else if (move_up_i > 0) {
    int i1 = move_up_i;
    int i2 = move_up_i - 1;
    std::swap((*profile_list)[i1], (*profile_list)[i2]);
  } else if (move_down_i >= 0) {
    int i1 = move_down_i;
    int i2 = move_down_i + 1;
    if (i2 < profile_list->size()) {
      std::swap((*profile_list)[i1], (*profile_list)[i2]);
    }
  } else if (copy_i >= 0) {
    *profile_list->Add() = (*profile_list)[copy_i];
  }

  if (ImGui::Button(std::format("Add {}", lower_type_name).c_str())) {
    profile_list->Add();
  }
}

}  // namespace aim

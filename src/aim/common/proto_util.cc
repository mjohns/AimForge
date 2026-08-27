#include "proto_util.h"

#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/message_differencer.h"

namespace aim {

bool IsEquivalentProto(const google::protobuf::Message& lhs, const google::protobuf::Message& rhs) {
  return google::protobuf::util::MessageDifferencer::Equivalent(lhs, rhs);
}

void ListUpdater::DrawMenuItems(int i) {
  if (ImGui::Selectable(std::format("{} Copy", icons::kContentCopy))) {
    copy = i;
  }
  if (ImGui::Selectable(std::format("{} Move up", icons::kArrowUpward))) {
    move_up = i;
  }
  if (ImGui::Selectable(std::format("{} Move down", icons::kArrowDownward))) {
    move_down = i;
  }
  ImGui::SpacedSeparator();
  if (ImGui::Selectable(std::format("{} Delete", icons::kDelete))) {
    remove = i;
  }
}

}  // namespace aim

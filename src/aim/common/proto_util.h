#pragma once

#include <algorithm>

#include "google/protobuf/message.h"

namespace aim {

bool IsEquivalentProto(const google::protobuf::Message& lhs, const google::protobuf::Message& rhs);

template <typename T>
bool IsDefaultInstance(const T& message) {
  return IsEquivalentProto(message, message.default_instance());
}

// Insert an element into a repeated field like proto.mutable_foo() at a given index.
template <typename T, typename R>
void InsertAtIndex(R* repeated_field, const T& value, int index) {
  if (index < 0) {
    index = 0;
  }
  bool is_last = index >= repeated_field->size();
  // Add at the end and then rotate down to correct index if necessary.
  *repeated_field->Add() = value;
  if (!is_last) {
    std::rotate(repeated_field->begin() + index, repeated_field->end() - 1, repeated_field->end());
  }
}

// Simplfies draw an basic editable list with menu items to delete/copy and move items.
struct ListUpdater {
  int remove = -1;
  int copy = -1;
  int move_up = -1;
  int move_down = -1;

  template <typename T>
  void Update(T* list) {
    if (remove >= 0) {
      list->erase(list->begin() + remove);
    } else if (move_up > 0) {
      int i1 = move_up;
      int i2 = move_up - 1;
      std::swap((*list)[i1], (*list)[i2]);
    } else if (move_down >= 0) {
      int i1 = move_down;
      int i2 = move_down + 1;
      if (i2 < list->size()) {
        std::swap((*list)[i1], (*list)[i2]);
      }
    } else if (copy >= 0) {
      InsertAtIndex(list, (*list)[copy], copy);
    }
  }

  void DrawMenuItems(int i);
};

}  // namespace aim

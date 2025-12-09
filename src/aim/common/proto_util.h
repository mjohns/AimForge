#pragma once

#include "google/protobuf/message.h"
#include "google/protobuf/util/message_differencer.h"

namespace aim {

template <typename T>
bool IsDefaultInstance(const T& message) {
  return google::protobuf::util::MessageDifferencer::Equivalent(message,
                                                                message.default_instance());
}

}  // namespace aim
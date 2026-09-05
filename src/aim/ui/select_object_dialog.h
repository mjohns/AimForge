#pragma once

#include <string>
#include <vector>

#include "aim/common/object_type.h"

namespace aim {

class SelectObjectDialog {
 public:
  virtual ~SelectObjectDialog() {}

  struct Result {
    std::vector<std::string> selected_objects;
  };

  virtual bool Draw(Result* result) = 0;
  virtual void NotifyOpen() = 0;
};

std::unique_ptr<SelectObjectDialog> CreateSelectObjectDialog(const std::string& id, ObjectType type);

}  // namespace aim

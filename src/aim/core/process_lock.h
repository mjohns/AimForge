#pragma once

#include <memory>
#include <optional>

#include "aim/common/simple_types.h"

namespace aim {

i64 GetPid();
std::string GetProcessNameFromPid(i64 pid);

// Class to ensure that only one instance of the program is running and accessing/writing to the
// database.
class ProcessLock {
 public:
  virtual ~ProcessLock() {}

  // Returns an existing process that holds the lock or empty if this process has succesfully
  // created the lock.
  virtual std::optional<i64> CreateLockFile() = 0;

  virtual void ReleaseLockFile() = 0;
};

std::unique_ptr<ProcessLock> CreateProcessLock();

}  // namespace aim

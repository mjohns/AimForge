#include "process_lock.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <optional>

namespace aim {
namespace {

i64 GetPid() {
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return getpid();
#endif
}

class ProcessLockImpl : public ProcessLock {
 public:
  ProcessLockImpl() : pid_(GetPid()) {}

  // Returns an existing process that holds the lock or empty if this process has succesfully
  // created the lock.
  std::optional<i64> CreateLockFile() override {
    return {};
  }

  void ReleaseLockFile() override {}

 private:
  i64 pid_;
};

}  // namespace

std::unique_ptr<ProcessLock> CreateProcessLock() {
  return std::make_unique<ProcessLockImpl>();
}

}  // namespace aim
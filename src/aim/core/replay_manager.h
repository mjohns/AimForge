#pragma once

#include <memory>

#include "aim/common/simple_types.h"

namespace aim {
class Replay;

class ReplayManager {
 public:
  virtual ~ReplayManager() {}

  virtual std::shared_ptr<Replay> GetReplay(i64 run_id)= 0;
  virtual void AddReplay(i64 run_id, std::shared_ptr<Replay> replay) = 0;
};

std::unique_ptr<ReplayManager> CreateReplayManager();

}  // namespace aim

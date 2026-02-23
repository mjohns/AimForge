#include "replay_manager.h"

#include <memory>
#include <vector>

#include "aim/scenario/replay.h"

namespace aim {
namespace {

constexpr const int kMaxSmallReplays = 20;

struct ReplayEntry {
  i64 run_id = -1;
  std::shared_ptr<Replay> replay;
};

class ReplayManagerImpl : public ReplayManager {
 public:
  std::shared_ptr<Replay> GetReplay(i64 run_id) override {
    if (large_replay_.run_id == run_id) {
      return large_replay_.replay;
    }
    for (const ReplayEntry& entry : small_replays_) {
      if (entry.run_id == run_id) {
        return entry.replay;
      }
    }
    return nullptr;
  }

  void AddReplay(i64 run_id, std::shared_ptr<Replay> replay) override {
    float approximate_size_mb = replay->GetApproximateSizeMb();
    bool is_large = approximate_size_mb > 0.2;
    ReplayEntry entry;
    entry.run_id = run_id;
    entry.replay = replay;

    if (is_large) {
      large_replay_ = entry;
      return;
    }

    // See if we should truncate small replays
    if (small_replays_.size() >= kMaxSmallReplays) {
      // The entry at the front of the vector is the oldest.
      small_replays_.erase(small_replays_.begin());
    }

    small_replays_.push_back(entry);
  }

 private:
  std::vector<ReplayEntry> small_replays_;
  ReplayEntry large_replay_;
};

}  // namespace

std::unique_ptr<ReplayManager> CreateReplayManager() {
  return std::make_unique<ReplayManagerImpl>();
}

}  // namespace aim
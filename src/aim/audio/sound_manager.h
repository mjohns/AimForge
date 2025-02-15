#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>

#include "aim/audio/sound.h"

namespace aim {

class SoundManager {
 public:
  explicit SoundManager(const std::vector<std::filesystem::path>& sound_dirs);

  SoundManager& PlayKillSound();
  SoundManager& PlayHitSound();
  SoundManager& PlayShootSound();
  SoundManager& PlayMetronomeSound();

 private:
  Sound* kill_sound_ = nullptr;
  Sound* shoot_sound_ = nullptr;
  Sound* hit_sound_ = nullptr;
  Sound* metronome_sound_ = nullptr;
  std::unordered_map<std::string, std::unique_ptr<Sound>> sound_cache_;
  std::vector<std::filesystem::path> sound_dirs_;
};

}  // namespace aim

#pragma once

#include <unordered_map>

#include "aim/audio/sound.h"

namespace aim {

class SoundManager {
 public:
  SoundManager();

  SoundManager& PlayKillSound();
  SoundManager& PlayShootSound();
  SoundManager& PlayMetronomeSound();

 private:
  Sound* kill_sound_ = nullptr;
  Sound* shoot_sound_ = nullptr;
  Sound* metronome_sound_ = nullptr;
  std::unordered_map<std::string, std::unique_ptr<Sound>> sound_cache_;
};

}  // namespace aim

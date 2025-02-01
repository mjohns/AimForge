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
  Sound* _kill_sound = nullptr;
  Sound* _shoot_sound = nullptr;
  Sound* _metronome_sound = nullptr;
  std::unordered_map<std::string, std::unique_ptr<Sound>> _sound_cache;
};

}  // namespace aim

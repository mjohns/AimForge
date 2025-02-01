#include "sound_manager.h"

namespace aim {
namespace {

constexpr int kKillChannel = 1;
constexpr int kShootChannel = 2;
constexpr int kMetronomeChannel = 3;

}  // namespace

SoundManager::SoundManager() {
  std::string kill_sound_name = "kill_confirmed.ogg";
  std::string shoot_sound_name = "shoot.ogg";
  std::string metronome_sound_name = "short_bass.wav";

  auto kill_sound = Sound::Load(kill_sound_name);
  auto shoot_sound = Sound::Load(shoot_sound_name);
  auto metronome_sound = Sound::Load(metronome_sound_name);

  _kill_sound = kill_sound.get();
  _shoot_sound = shoot_sound.get();
  _metronome_sound = metronome_sound.get();

  _sound_cache[kill_sound_name] = std::move(kill_sound);
  _sound_cache[shoot_sound_name] = std::move(shoot_sound);
  _sound_cache[metronome_sound_name] = std::move(metronome_sound);
}

SoundManager& SoundManager::PlayKillSound() {
  if (_kill_sound != nullptr) {
    _kill_sound->Play(kKillChannel);
  }
  return *this;
}

SoundManager& SoundManager::PlayShootSound() {
  if (_shoot_sound != nullptr) {
    _shoot_sound->Play(kShootChannel);
  }
  return *this;
}

SoundManager& SoundManager::PlayMetronomeSound() {
  if (_metronome_sound != nullptr) {
    _metronome_sound->Play(kMetronomeChannel);
  }
  return *this;
}

}  // namespace aim
